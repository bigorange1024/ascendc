#!/usr/bin/env python3
"""gen_dk_pke.py — 与 Encrypt gen_ek_pke 同源 KeyGen golden，输出 dk 段 ŝ（1536B）。"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from gen_ek_pke import (
    K,
    N,
    POLY_D12_BYTES,
    build_a_hat,
    build_src,
    derand_bytes_from_seed,
    hash_g_rho_sigma,
    poly_byte_encode12,
    stage123_transform,
)

DK_BYTES = K * POLY_D12_BYTES


def build_dk_pke(seed_d: int) -> np.ndarray:
    d = derand_bytes_from_seed(seed_d)
    _, sigma = hash_g_rho_sigma(d)
    src = build_src(sigma)
    s = src[:K]
    s_hat = stage123_transform(s, "ntt")
    dk = bytearray(DK_BYTES)
    for j in range(K):
        dk[j * POLY_D12_BYTES : (j + 1) * POLY_D12_BYTES] = poly_byte_encode12(s_hat[j])
    return np.frombuffer(bytes(dk), dtype=np.uint8)


def main() -> None:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <SEED_D> <dk_pke.out>", file=sys.stderr)
        sys.exit(1)
    seed_d = int(sys.argv[1])
    out = Path(sys.argv[2])
    dk = build_dk_pke(seed_d)
    if dk.size != DK_BYTES:
        raise SystemExit(f"dk size {dk.size} != {DK_BYTES}")
    out.write_bytes(dk.tobytes())
    print(f"[gen_dk_pke] OK {DK_BYTES}B SEED_D={seed_d}")


if __name__ == "__main__":
    main()
