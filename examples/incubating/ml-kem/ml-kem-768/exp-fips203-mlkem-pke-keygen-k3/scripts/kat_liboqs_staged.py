#!/usr/bin/env python3
# @probe exp-fips203-mlkem-pke-keygen-k3
# @file scripts/kat_liboqs_staged.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `kat_liboqs_staged.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1184B) + dk_pke.bin (1152B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps. 本文件可能用于 legacy/staged I/O 或分阶段调试，非默认生产路径。 / May use legacy staging I/O.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify KEYGEN_KAT=1 bash run.sh 或 kat_*.sh；对比 liboqs。

# coding=utf-8
"""
本文件在 KeyGen 流水线中的位置：Host KAT 编排（分阶段门控 + liboqs）。
对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
与 golden 关系：仅 I/O 等价；liboqs 为外部 oracle，非 AscendC 规格。
文件：scripts/kat_liboqs_staged.py

分阶段 KAT：G0–G4 每段对 Host golden；G4 再对 liboqs。用于定位 example/探针分歧点。

门禁与 output/ 中间产物（run.sh 默认保留）：
  g0 — 仅 Host golden 自检 + golden↔liboqs
  g2 — presample 后 src.bin
  g3 — Â 后 a_hat.bin、rho.bin
  g4 — 全链 ek_pke / dk_pke + verify_result + liboqs

用法：
  KAT_SEED_D=20260619 python3 scripts/kat_liboqs_staged.py
  KAT_GATES=g0,g2,g3,g4 KAT_SEED_D=1 python3 scripts/kat_liboqs_staged.py
  KAT_RUN_MODE=cpu|sim  默认 cpu
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


import numpy as np

ROOT = Path(__file__).resolve().parent.parent
REPO_ROOT = _ascendc_repo_root(ROOT)
FIPS203 = REPO_ROOT / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(FIPS203))
sys.path.insert(0, str(ROOT / "scripts"))

from golden_se_sampling import derand_bytes_from_seed  # noqa: E402
from keygen_golden import build_full_keygen  # noqa: E402

EK_PKE_BYTES = 1184
DK_PKE_BYTES = 1152
REF_BIN = ROOT / "scripts" / "liboqs_pke_keygen_ref"
BUILD_REF = ROOT / "scripts" / "build_liboqs_pke_ref.sh"
OUT = ROOT / "output"
SOC = os.environ.get("KAT_SOC_VERSION", "Ascend910B4")
RUN_MODE = os.environ.get("KAT_RUN_MODE", "cpu")
DEFAULT_GATES = ("g0", "g2", "g3", "g4")


def _ensure_liboqs() -> Path:
    """若缺 liboqs 参考二进制则调用 build 脚本；返回可执行路径。
    """
    if not REF_BIN.is_file():
        subprocess.check_call(["bash", str(BUILD_REF)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return REF_BIN


def liboqs_pke(d: bytes) -> tuple[np.ndarray, np.ndarray]:
    """调用 liboqs PKE KeyGen 参考，返回 (ek_pke, dk_pke) uint8 数组。

    @param d  32B 确定性种子（与 derand_bytes_from_seed 一致）
    """
    with tempfile.TemporaryDirectory(prefix="stg_") as td:
        ek_p, dk_p = Path(td) / "ek", Path(td) / "dk"
        subprocess.check_call(
            [str(_ensure_liboqs()), str(ek_p), str(dk_p), d.hex()],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return np.fromfile(ek_p, dtype=np.uint8), np.fromfile(dk_p, dtype=np.uint8)


def _run_gate(gate: str, seed_d: int) -> None:
    """以 KEYGEN_GATE 调用探针 run.sh（cpu/sim 由 KAT_RUN_MODE 决定）。
    """
    env = os.environ.copy()
    env["SEED_D"] = str(seed_d)
    env["KEYGEN_GATE"] = gate
    subprocess.check_call(["bash", "run.sh", "-r", RUN_MODE, "-v", SOC], cwd=ROOT, env=env)


def _verify_gate(gate: str) -> None:
    """对当前门控产物跑 scripts/verify_result.py。
    """
    env = os.environ.copy()
    env["KEYGEN_GATE"] = gate
    subprocess.check_call([sys.executable, str(ROOT / "scripts/verify_result.py")], cwd=ROOT, env=env)


def _cmp_bytes(name: str, got: np.ndarray, exp: np.ndarray) -> None:
    """字节级断言；失败打印首个差异下标。
    """
    if got.shape != exp.shape or not np.array_equal(got, exp):
        idx = int(np.argmax(got != exp)) if got.size == exp.size else 0
        raise SystemExit(f"[staged FAIL] {name} @ {idx}: got={got.flat[idx] if got.size else '?'} expect={exp.flat[idx] if exp.size else '?'}")


def stage_g0(seed_d: int, d: bytes, kg: dict) -> None:
    """G0：Host golden 自检 + golden↔liboqs（不跑设备核）。
    """
    _run_gate("g0", seed_d)
    _verify_gate("g0")
    ek_oqs, dk_oqs = liboqs_pke(d)
    _cmp_bytes("g0 liboqs ek_pke", ek_oqs, kg["ek_pke"])
    _cmp_bytes("g0 liboqs dk_pke", dk_oqs, kg["dk_pke"])
    print("[staged] g0 PASS (host golden + liboqs)")


def stage_g2(seed_d: int, kg: dict) -> None:
    """G2：presample 后门控，对拍 src.bin。
    """
    _ = kg
    _run_gate("g2", seed_d)
    _verify_gate("g2")
    print("[staged] g2 PASS (src vs golden via verify_result)")


def _check_a_hat_rows() -> None:
    """G3 后逐行 a_hat 诊断：D13 Â[9,256]，双 AIV 分片 5+4。"""
    a = np.fromfile(OUT / "a_hat.bin", dtype=np.int32).reshape(9, 256)
    g = np.fromfile(OUT / "golden_a_hat.bin", dtype=np.int32).reshape(9, 256)
    bad = []
    for r in range(9):
        d = int(np.abs(a[r].astype(np.int64) - g[r].astype(np.int64)).max())
        if d:
            bad.append(f"row{r}={d}")
    if bad:
        raise SystemExit(f"[staged FAIL] a_hat: {', '.join(bad)}")


def stage_g3(seed_d: int, kg: dict) -> None:
    """G3：Â 生成后门控；额外逐行诊断 a_hat。
    """
    _ = kg
    _run_gate("g3", seed_d)
    _verify_gate("g3")
    _check_a_hat_rows()
    print("[staged] g3 PASS (a_hat rows 0–8, rho via verify_result)")


def stage_g4(seed_d: int, d: bytes, kg: dict) -> None:
    """G4：全链 ek_pke/dk_pke 对 golden 与 liboqs。
    """
    _run_gate("g4", seed_d)
    _verify_gate("g4")
    ek = np.fromfile(OUT / "ek_pke.bin", dtype=np.uint8)
    dk = np.fromfile(OUT / "dk_pke.bin", dtype=np.uint8)
    _cmp_bytes("g4 ek_pke vs golden", ek, kg["ek_pke"])
    _cmp_bytes("g4 dk_pke vs golden", dk, kg["dk_pke"])
    ek_oqs, dk_oqs = liboqs_pke(d)
    _cmp_bytes("g4 ek_pke vs liboqs", ek, ek_oqs)
    _cmp_bytes("g4 dk_pke vs liboqs", dk, dk_oqs)
    print("[staged] g4 PASS (device + golden + liboqs)")


STAGES = {"g0": stage_g0, "g2": stage_g2, "g3": stage_g3, "g4": stage_g4}


def main() -> int:
    """按 KAT_GATES 依次跑门控；默认 g0,g2,g3,g4。

    @return 0 全过
    """
    seed_d = int(os.environ.get("KAT_SEED_D", "20260619"))
    gates = tuple(os.environ.get("KAT_GATES", ",".join(DEFAULT_GATES)).split(","))
    d = derand_bytes_from_seed(seed_d)
    kg = build_full_keygen(seed_d)
    print(f"[staged] SEED_D={seed_d} gates={gates} RUN_MODE={RUN_MODE}")
    for g in gates:
        g = g.strip().lower()
        if g not in STAGES:
            raise SystemExit(f"unknown gate {g!r}; use g0,g2,g3,g4")
        if g == "g0":
            STAGES[g](seed_d, d, kg)
        elif g == "g4":
            STAGES[g](seed_d, d, kg)
        else:
            STAGES[g](seed_d, kg)
    print(f"[staged] ALL PASS seed_d={seed_d}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
