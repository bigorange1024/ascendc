#!/usr/bin/env python3
"""
gen_data.py — KEM_KEYGEN_VERIFY=1 时生成 host golden（黑盒 oracle，非设备规格）。

使用与 device 一致的 d/z 域分离 + liboqs keypair_derand 产出期望 ek_kem/dk_kem。
"""
from __future__ import annotations

import hashlib
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
FIPS203_SE = REPO / "library/shared/fips203_se_sample"
sys.path.insert(0, str(FIPS203_SE))
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

EK_KEM_BYTES = 1568
DK_KEM_BYTES = 3168
SEED_D_DEFAULT = 20260619

BUILD_REF = REPO / "scripts/build_liboqs_kem_ref.sh"
REF_BIN = REPO / "scripts/liboqs_kem_ref"


def derand_z_from_seed(seed_d: int) -> bytes:
    msg = f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def _ensure_ref() -> Path:
    if REF_BIN.is_file():
        return REF_BIN
    subprocess.check_call(["bash", str(BUILD_REF)])
    return REF_BIN


def main() -> None:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    d = derand_bytes_from_seed(seed_d)
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

    print(f"[gen_data] SEED_D={seed_d} golden ek_kem/dk_kem via liboqs derand (64B d||z)")


if __name__ == "__main__":
    main()
