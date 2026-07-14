#!/usr/bin/env python3
"""
gen_data.py — Alg.19 KEM KeyGen 的 host golden 生成器（黑盒 oracle，非设备规格）。

用途（仅 KEM_KEYGEN_VERIFY=1 / 对拍路径）：
  用与 device 一致的 d/z 域分离消息，拼 64B kem_seed=d‖z，
  再调仓库 scripts/liboqs_kem_ref keypair_derand，写出
  output/golden_ek_kem.bin、golden_dk_kem.bin。

禁止：把本脚本逻辑当作 AscendC 必须复刻的实现；生产默认 run.sh
  不依赖 liboqs（见 SELF_CONTAINED.md）。
"""
from __future__ import annotations

import hashlib
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# examples/incubating/exp-*/ → 仓库根
REPO = ROOT.parent.parent.parent
FIPS203_SE = REPO / "library/shared/fips203_se_sample"
sys.path.insert(0, str(FIPS203_SE))
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

EK_KEM_BYTES = 1568
DK_KEM_BYTES = 3168
# 定点复现可 export SEED_D=20260619；默认见 resolve_host_seed_d.py（SHA3 派生）

BUILD_REF = REPO / "scripts/build_liboqs_kem_ref.sh"
REF_BIN = REPO / "scripts/liboqs_kem_ref"


def derand_z_from_seed(seed_d: int) -> bytes:
    """与设备 DerandZFromSeedD 同式：SHA3-256("exp-mlkem-f203-kem-k4:SEED_Z="‖十进制)。"""
    msg = f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def _ensure_ref() -> Path:
    """确保 liboqs_kem_ref 可执行存在；缺失则跑仓库构建脚本。"""
    if REF_BIN.is_file():
        return REF_BIN
    subprocess.check_call(["bash", str(BUILD_REF)])
    return REF_BIN


def main() -> None:
    """写 seed_d.bin，并生成 golden ek/dk（liboqs derand）。"""
    from resolve_host_seed_d import resolve_host_seed_d

    seed_d, how = resolve_host_seed_d()
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

    ref = _ensure_ref()
    ek_path = out_dir / "golden_ek_kem.bin"
    dk_path = out_dir / "golden_dk_kem.bin"
    hex_seed = kem_seed.hex()
    subprocess.check_call([str(ref), "keygen", str(ek_path), str(dk_path), hex_seed])

    print(f"[gen_data] SEED_D={seed_d} via={how} golden ek_kem/dk_kem via liboqs derand (64B d||z)")


if __name__ == "__main__":
    main()
