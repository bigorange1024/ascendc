#!/usr/bin/env python3
"""
gen_data.py — Alg.19 KEM KeyGen 的 host golden 生成器（黑盒 oracle，非设备规格）。

用途（仅 KEM_KEYGEN_VERIFY=1 / 对拍路径）：
  用与 device 一致的 d/z 域分离消息，拼 64B kem_seed=d‖z，
  交 library/shared/f203_kem_ref 写出 output/golden_ek_kem.bin、golden_dk_kem.bin。

golden 后端（2026-07-31）：**liboqs 优先，缺失则回落** PKE KeyGen golden + H/拼装。
  背景：借入实机装不了 thirdparty/liboqs，原先硬依赖会让本探针直接跑不起来。
  本探针未 vendored PKE KeyGen golden，回落件借 stable KEM KeyGen 算子的 keygen_golden
  （与 Alg.21 探针借 stable Encrypt host_golden 同例）。

禁止：把本脚本逻辑当作 AscendC 必须复刻的实现。
"""
from __future__ import annotations

import hashlib
import os
import struct
import sys
from pathlib import Path

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


ROOT = Path(__file__).resolve().parent.parent
REPO = _ascendc_repo_root(ROOT)
FIPS203_SE = REPO / "library/shared/fips203_se_sample"
KEM_REF = REPO / "library/shared/f203_kem_ref"
# 回落路径的 PKE KeyGen golden：借 stable KEM KeyGen 算子（自带 vendored LUT，不依赖 thirdparty/）
STABLE_KEM_KG = REPO / "examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/scripts"
for _p in (FIPS203_SE, KEM_REF):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402
import kem_ref  # noqa: E402

EK_KEM_BYTES = 1568
DK_KEM_BYTES = 3168
SEED_D_DEFAULT = 20260619


def derand_z_from_seed(seed_d: int) -> bytes:
    """与设备 DerandZFromSeedD 同式：SHA3-256("exp-mlkem-f203-kem-k4:SEED_Z="‖十进制)。"""
    msg = f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def _make_pke_keygen(seed_d: int):
    """
    造 kem_ref 回落路径要的 PKE KeyGen golden：d(32B) → (ek_pke 1568B, dk_pke 1536B)。

    stable keygen_golden 以 seed_d 为入口（内部同样 d = derand_bytes_from_seed(seed_d)），
    故闭包住 seed_d 并断言 d 一致，避免 d 与 seed_d 走岔产出错 golden。
    仅在无 liboqs 时才真正 import，避免正常机器上多加载一棵 stable 脚本树。
    """

    def _pke_keygen(d: bytes) -> "tuple[bytes, bytes]":
        if d != derand_bytes_from_seed(seed_d):
            raise SystemExit("[gen_data] d 与 SEED_D 不一致，拒绝生成 golden")
        import importlib.util

        spec = importlib.util.spec_from_file_location(
            "stable_kem_keygen_golden", STABLE_KEM_KG / "keygen_golden.py"
        )
        if spec is None or spec.loader is None:
            raise SystemExit(f"[gen_data] 无法加载 {STABLE_KEM_KG / 'keygen_golden.py'}")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        kg = mod.build_full_keygen(seed_d)
        return kg["ek_pke"].tobytes(), kg["dk_pke"].tobytes()

    return _pke_keygen


def main() -> None:
    """写 seed_d.bin，并生成 golden ek/dk（liboqs 优先，缺失回落 PKE KeyGen golden + 拼装）。"""
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    # d：与 vendor prep DerandFromSeedD 对齐（library/shared golden_se_sampling）
    d = derand_bytes_from_seed(seed_d)
    # z：与本仓 kem/f203_kem_kg_derand_ub.hpp 对齐
    z = derand_z_from_seed(seed_d)
    kem_seed = d + z

    out_dir = ROOT / "output"
    out_dir.mkdir(exist_ok=True)
    inp = ROOT / "input"
    inp.mkdir(exist_ok=True)
    (inp / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))

    ek_path = out_dir / "golden_ek_kem.bin"
    dk_path = out_dir / "golden_dk_kem.bin"
    src = kem_ref.kem_keygen(kem_seed, ek_path, dk_path, pke_keygen=_make_pke_keygen(seed_d))

    print(f"[gen_data] SEED_D={seed_d} golden ek_kem/dk_kem via {src} derand (64B d||z)")


if __name__ == "__main__":
    main()
