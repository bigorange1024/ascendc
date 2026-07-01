#!/usr/bin/env python3
"""
liboqs_kem_fixture.py — 生成 liboqs KEM KeyGen 黑盒向量（ml_kem_1024 / k=4）。

device 可复现路径：d=DerandFromSeedD(seed_d)，z=DerandZFromSeedD(seed_d)，kem_seed=d||z。
"""
from __future__ import annotations

import argparse
import hashlib
import os
import struct
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
FIPS203_SE = REPO_ROOT / "library/shared/fips203_se_sample"
REF_BIN = REPO_ROOT / "scripts/liboqs_kem_ref"
BUILD_REF = REPO_ROOT / "scripts/build_liboqs_kem_ref.sh"

sys.path.insert(0, str(FIPS203_SE))
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

EK_BYTES = 1568
DK_BYTES = 3168
SEED_D_DEFAULT = 20260619


def derand_z_from_seed(seed_d: int) -> bytes:
    msg = f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def _ensure_ref() -> Path:
    if REF_BIN.is_file():
        return REF_BIN
    subprocess.check_call(["bash", str(BUILD_REF)])
    return REF_BIN


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed-d", type=int, default=int(os.environ.get("SEED_D", SEED_D_DEFAULT)))
    ap.add_argument("--out-dir", type=Path, required=True)
    args = ap.parse_args()

    d = derand_bytes_from_seed(args.seed_d)
    z = derand_z_from_seed(args.seed_d)
    kem_seed = d + z

    out = args.out_dir
    out.mkdir(parents=True, exist_ok=True)
    (out / "seed_d.bin").write_bytes(struct.pack("<I", args.seed_d))
    # fixture 可含 d/z 供调试；不进探针生产 run.sh
    (out / "d.bin").write_bytes(d)
    (out / "z.bin").write_bytes(z)
    (out / "kem_seed.bin").write_bytes(kem_seed)

    ref = _ensure_ref()
    subprocess.check_call(
        [str(ref), "keygen", str(out / "ek_kem.bin"), str(out / "dk_kem.bin"), kem_seed.hex()]
    )
    print(f"[liboqs_kem_fixture] SEED_D={args.seed_d} -> {out}")


if __name__ == "__main__":
    main()
