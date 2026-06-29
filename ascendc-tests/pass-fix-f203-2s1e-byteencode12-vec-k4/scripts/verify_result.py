#!/usr/bin/env python3
# coding=utf-8
"""ByteEncode₁₂-only 对拍：输入契约同 Alg.13 mixPass=7 preset；验 ek/sk。"""
import os
import sys

import numpy as np

N = 256
K_S = 4
K_HAT = 4
K_DST = 12
POLYVEC_BYTES = K_HAT * 384


def check(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    diff = np.abs(got.astype(np.int64) - ref.astype(np.int64))
    mx = int(diff.max())
    if mx != 0:
        print(f"[FAIL] {label} max_abs_diff={mx}")
        return 1
    print(f"[SUCCESS] {label} max_abs_diff=0")
    return 0


def check_bytes(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    if got.shape != ref.shape:
        print(f"[FAIL] {label} shape {got.shape} vs {ref.shape}")
        return 1
    diff = np.abs(got.astype(np.int16) - ref.astype(np.int16))
    mx = int(diff.max())
    if mx != 0:
        flat = int(diff.argmax())
        print(f"[FAIL] {label} max_abs_diff={mx} byte_idx={flat}")
        return 1
    print(f"[SUCCESS] {label} max_abs_diff=0")
    return 0


def main() -> int:
    rc = 0

    if os.path.isfile("./output/golden_dst.bin") and os.path.isfile("./input/dst.bin"):
        g = np.fromfile("./output/golden_dst.bin", dtype=np.int32).reshape(K_DST, N)
        d = np.fromfile("./input/dst.bin", dtype=np.int32).reshape(K_DST, N)
        rc |= check("input dst vs golden_dst (12×256 preset)", d, g)
        rc |= check("dst s_hat_aiv0 vs s_hat_aiv1", d[:K_S], d[K_S : 2 * K_S])

    if os.path.isfile("./output/golden_t_hat.bin") and os.path.isfile("./input/t_hat.bin"):
        g = np.fromfile("./output/golden_t_hat.bin", dtype=np.int32).reshape(K_HAT, N)
        t = np.fromfile("./input/t_hat.bin", dtype=np.int32).reshape(K_HAT, N)
        rc |= check("input t_hat vs golden_t_hat (4×256 preset)", t, g)

    if os.path.isfile("./output/golden_ek_polyvec.bin") and os.path.isfile("./output/ek_polyvec.bin"):
        g = np.fromfile("./output/golden_ek_polyvec.bin", dtype=np.uint8)
        e = np.fromfile("./output/ek_polyvec.bin", dtype=np.uint8)
        rc |= check_bytes("ek_polyvec ByteEncode₁₂(t̂)", e, g)

    if os.path.isfile("./output/golden_sk_polyvec.bin") and os.path.isfile("./output/sk_polyvec.bin"):
        g = np.fromfile("./output/golden_sk_polyvec.bin", dtype=np.uint8)
        s = np.fromfile("./output/sk_polyvec.bin", dtype=np.uint8)
        rc |= check_bytes("sk_polyvec ByteEncode₁₂(ŝ)", s, g)

    if rc == 0:
        print("[verify] PASS")
    return rc


if __name__ == "__main__":
    sys.exit(main())
