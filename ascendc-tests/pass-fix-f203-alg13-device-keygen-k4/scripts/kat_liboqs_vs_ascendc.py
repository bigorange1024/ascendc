#!/usr/bin/env python3
# @probe pass-fix-f203-alg13-device-keygen-k4
# @file scripts/kat_liboqs_vs_ascendc.py
# @layer script
# @role liboqs KAT 与 AscendC 输出对比（KEYGEN_KAT=1 静默路径）。 / liboqs KAT harness.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify KEYGEN_KAT=1 bash run.sh 或 kat_*.sh；对比 liboqs。

# coding=utf-8
"""
本文件在 KeyGen 流水线中的位置：Host：KeyGen 输入、golden、KAT、验收脚本。
对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
文件：scripts/kat_liboqs_vs_ascendc.py
"""

from __future__ import annotations

"""liboqs PKE KeyGen ↔ 探针 AscendC 对拍（pass-fix-f203-alg13-device-keygen-k4）。

种子契约（与 run.sh / 设备 DerandFromSeedD 一致）：
  SEED_D → d = SHA3-256("exp-mlkem-f203-2s1e-k4:SEED_D=<decimal>")
  liboqs：ml_kem_1024 indcpa coins[0:32]
  AscendC：生产 I/O 全链（**2 launch**：prep | compute+ek‖ρ 内核融合）

用法：
  bash kat_liboqs_vs_ascendc.sh
  KAT_CPU_COUNT=5 KAT_SIM_COUNT=1 bash kat_liboqs_vs_ascendc.sh
  KAT_SEEDS="20260619,123,456,..." bash kat_liboqs_vs_ascendc.sh  # 11 个整数，前 10 CPU、后 1 SIM
  KAT_VERBOSE=1 bash kat_liboqs_vs_ascendc.sh
"""

import os
import secrets
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
FIPS203_SE_SCRIPTS = ROOT / "scripts" / "prep" / "fips203_se_sample"
sys.path.insert(0, str(FIPS203_SE_SCRIPTS))
sys.path.insert(0, str(ROOT / "scripts"))

from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

EK_PKE_BYTES = 1568
DK_PKE_BYTES = 1536
REF_BIN = ROOT / "scripts" / "liboqs_pke_keygen_ref"
BUILD_REF_SH = ROOT / "scripts" / "build_liboqs_pke_ref.sh"
SOC_VERSION = os.environ.get("KAT_SOC_VERSION", "Ascend910B4")
VERBOSE = os.environ.get("KAT_VERBOSE", "0") == "1"
LOG_PATH = Path(os.environ.get("KAT_LOG", str(ROOT / "output" / "kat_liboqs_vs_ascendc.log")))


# 本函数为 KeyGen 流水线组件 `_fail`（详见 STATUS/customspec）。
def _fail(label: str, round_no: int, total: int, seed_d: int, msg: str) -> None:
  raise SystemExit(f"[KAT FAIL] {label} round {round_no}/{total} seed_d={seed_d}: {msg}")


def _ensure_liboqs_ref() -> Path:
    if REF_BIN.is_file():
        return REF_BIN
    kw = {} if VERBOSE else {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
    subprocess.check_call(["bash", str(BUILD_REF_SH)], **kw)
    return REF_BIN


def random_seed_d() -> int:
    return secrets.randbelow(2**32)


def liboqs_pke_keygen(d: bytes) -> tuple[np.ndarray, np.ndarray]:
    ref = _ensure_liboqs_ref()
    with tempfile.TemporaryDirectory(prefix="oqs_kat_") as td:
        td_path = Path(td)
        ek_path = td_path / "ek_oqs.bin"
        dk_path = td_path / "dk_oqs.bin"
        subprocess.check_call(
            [str(ref), str(ek_path), str(dk_path), d.hex()],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        ek = np.fromfile(ek_path, dtype=np.uint8)
        dk = np.fromfile(dk_path, dtype=np.uint8)
    if ek.size != EK_PKE_BYTES or dk.size != DK_PKE_BYTES:
        raise SystemExit(f"liboqs size ek={ek.size} dk={dk.size}")
    return ek, dk


# 本函数为 KeyGen 流水线组件 `run_ascendc_keygen`（详见 STATUS/customspec）。
def run_ascendc_keygen(seed_d: int, run_mode: str) -> tuple[np.ndarray, np.ndarray]:
  env = os.environ.copy()
  env["SEED_D"] = str(seed_d)
  env["KEYGEN_KAT"] = "0" if VERBOSE else "1"
  cmd = ["bash", "run.sh", "-r", run_mode, "-v", SOC_VERSION]
  if VERBOSE:
    rc = subprocess.run(cmd, cwd=ROOT, env=env).returncode
  else:
    LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
    with LOG_PATH.open("a", encoding="utf-8") as logf:
      logf.write(f"\n{'='*72}\n[kat] keygen {run_mode} seed_d={seed_d}\n")
      logf.flush()
    rc = subprocess.run(cmd, cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
  if rc != 0:
    raise SystemExit(f"run.sh({run_mode}) exit={rc} seed_d={seed_d} — see {LOG_PATH}")
  ek = np.fromfile(ROOT / "output" / "ek_pke.bin", dtype=np.uint8)
  dk = np.fromfile(ROOT / "output" / "dk_pke.bin", dtype=np.uint8)
  if ek.size != EK_PKE_BYTES or dk.size != DK_PKE_BYTES:
    raise SystemExit(f"g4 output size ek={ek.size} dk={dk.size} seed_d={seed_d}")
  return ek, dk


# 本函数为 KeyGen 流水线组件 `compare_pke`（详见 STATUS/customspec）。
def compare_pke(round_no: int, total: int, seed_d: int, label: str, ek_oqs, dk_oqs, ek_dev, dk_dev) -> None:
  if not np.array_equal(ek_dev, ek_oqs):
    idx = int(np.argmax(ek_dev != ek_oqs))
    _fail(label, round_no, total, seed_d, f"ek_pke @ {idx}: ascendc={ek_dev.flat[idx]} liboqs={ek_oqs.flat[idx]}")
  if not np.array_equal(dk_dev, dk_oqs):
    idx = int(np.argmax(dk_dev != dk_oqs))
    _fail(label, round_no, total, seed_d, f"dk_pke @ {idx}: ascendc={dk_dev.flat[idx]} liboqs={dk_oqs.flat[idx]}")


def one_round(seed_d: int, run_mode: str, label: str, round_no: int, total: int) -> None:
  d = derand_bytes_from_seed(seed_d)
  ek_oqs, dk_oqs = liboqs_pke_keygen(d)
  ek_dev, dk_dev = run_ascendc_keygen(seed_d, run_mode)
  compare_pke(round_no, total, seed_d, label, ek_oqs, dk_oqs, ek_dev, dk_dev)


# 本函数为 KeyGen 流水线组件 `parse_seed_list`（详见 STATUS/customspec）。
def parse_seed_list() -> tuple[list[int], list[int]]:
  cpu_count = int(os.environ.get("KAT_CPU_COUNT", "10"))
  sim_count = int(os.environ.get("KAT_SIM_COUNT", "1"))
  if cpu_count < 1 or sim_count < 1:
    raise SystemExit("KAT_CPU_COUNT and KAT_SIM_COUNT must be >= 1")
  raw = os.environ.get("KAT_SEEDS", "").strip()
  if raw:
    seeds = [int(x.strip()) for x in raw.split(",") if x.strip()]
    need = cpu_count + sim_count
    if len(seeds) < need:
      raise SystemExit(f"KAT_SEEDS has {len(seeds)} values, need {need} (CPU×{cpu_count} + SIM×{sim_count})")
    return seeds[:cpu_count], seeds[cpu_count : cpu_count + sim_count]
  cpu_seeds = [random_seed_d() for _ in range(cpu_count)]
  sim_seeds = [random_seed_d() for _ in range(sim_count)]
  return cpu_seeds, sim_seeds


# 本函数为 KeyGen 流水线组件 `main`（详见 STATUS/customspec）。
def main() -> int:
  cpu_count = int(os.environ.get("KAT_CPU_COUNT", "10"))
  sim_count = int(os.environ.get("KAT_SIM_COUNT", "1"))
  _ensure_liboqs_ref()
  if not VERBOSE:
    LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
    LOG_PATH.write_text("[kat] probe keygen vs liboqs\n", encoding="utf-8")
    print(f"[kat] quiet — logs -> {LOG_PATH}")
  cpu_seeds, sim_seeds = parse_seed_list()
  with LOG_PATH.open("a", encoding="utf-8") as logf:
    logf.write(f"cpu seeds ({cpu_count}): {cpu_seeds}\nsim seeds ({sim_count}): {sim_seeds}\n")
  print(f"[kat] start CPU×{cpu_count} + SIM×{sim_count} (同 SEED_D：liboqs d=SHA3-256(msg) ↔ 探针 DerandFromSeedD)")
  os.environ.setdefault("KEYGEN_SKIP_REBUILD", "0")
  for i, s in enumerate(cpu_seeds):
    one_round(s, "cpu", "CPU", i + 1, cpu_count)
    if i == 0:
      os.environ["KEYGEN_SKIP_REBUILD"] = "1"
  print(f"[kat] CPU {cpu_count}/{cpu_count} OK", flush=True)
  for i, s in enumerate(sim_seeds):
    os.environ.pop("KEYGEN_SKIP_REBUILD", None)
    one_round(s, "sim", "SIM", i + 1, sim_count)
  print(f"[kat] SIM {sim_count}/{sim_count} OK", flush=True)
  print(f"[kat] PASS CPU×{cpu_count} + SIM×{sim_count} (probe keygen vs liboqs)")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
