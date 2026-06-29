#!/usr/bin/env python3
# coding=utf-8
"""Golden：SHAKE256 轨 prf_out[8,128] + golden_src[8,256]；仅写 SEED_D 到 input/。"""
from __future__ import annotations

import os
import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
FIPS203_SE_SCRIPTS = Path(__file__).resolve().parents[2] / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(FIPS203_SE_SCRIPTS))

from golden_se_sampling import (  # noqa: E402
    build_src,
    derand_bytes_from_seed,
    hash_g_sigma,
    prf_shake256,
)

SEED_D_DEFAULT = 20260619
PRF_ROWS = 8
PRF_BYTES = 128


def build_prf_out(seed_d: int) -> np.ndarray:
    sigma = hash_g_sigma(derand_bytes_from_seed(seed_d))
    rows = [np.frombuffer(prf_shake256(sigma, nonce), dtype=np.uint8) for nonce in range(PRF_ROWS)]
    out = np.stack(rows)
    if out.shape != (PRF_ROWS, PRF_BYTES):
        raise SystemExit(f"unexpected prf_out shape {out.shape}")
    return out


def main() -> None:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    os.environ["FIPS203_PRF_BACKEND"] = "shake256"

    prf_out = build_prf_out(seed_d)
    src = build_src(seed_d)
    if src.shape != (8, 256):
        raise SystemExit(f"unexpected src shape {src.shape}")

    input_dir = ROOT / "input"
    output_dir = ROOT / "output"
    input_dir.mkdir(exist_ok=True)
    output_dir.mkdir(exist_ok=True)

    (input_dir / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    prf_out.tofile(output_dir / "golden_prf_out.bin")
    src.astype(np.int32).tofile(output_dir / "golden_src.bin")

    print(f"[gen_data] SEED_D={seed_d} FIPS203_PRF_BACKEND=shake256")
    print(f"[gen_data] wrote input/seed_d.bin")
    print(f"[gen_data] wrote output/golden_prf_out.bin shape={prf_out.shape}")
    print(f"[gen_data] wrote output/golden_src.bin shape={src.shape}")


if __name__ == "__main__":
    main()
