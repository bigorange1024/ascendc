#!/usr/bin/env python3
"""gen_data — RB-T05 ByteDecode₁₂：随机 t̂[k·256] → BE₁₂ 载荷 + golden。

Host oracle（非 AscendC 规格）：FIPS 203 Alg.5/6 d=12，k=4。
  2 系数 → 3 字节：b0=v0&0xFF；b1=((v0>>8)&0xF)|((v1&0xF)<<4)；b2=(v1>>4)&0xFF
"""
from __future__ import annotations

import os

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))

K = 4
N = 256
Q = 3329
POLY_BYTES = 384
SEED = 20260908


def byte_encode12(coeffs: np.ndarray) -> np.ndarray:
    """coeffs int32[256] → uint8[384]。"""
    assert coeffs.shape == (N,)
    out = np.zeros(POLY_BYTES, dtype=np.uint8)
    for i in range(N // 2):
        v0 = int(coeffs[2 * i]) & 0xFFF
        v1 = int(coeffs[2 * i + 1]) & 0xFFF
        out[3 * i + 0] = v0 & 0xFF
        out[3 * i + 1] = ((v0 >> 8) & 0x0F) | ((v1 & 0x0F) << 4)
        out[3 * i + 2] = (v1 >> 4) & 0xFF
    return out


def byte_decode12(buf: np.ndarray) -> np.ndarray:
    """uint8[384] → int32[256]（golden 自洽校验用）。"""
    assert buf.shape == (POLY_BYTES,)
    out = np.zeros(N, dtype=np.int32)
    for i in range(N // 2):
        b0 = int(buf[3 * i + 0])
        b1 = int(buf[3 * i + 1])
        b2 = int(buf[3 * i + 2])
        out[2 * i] = b0 | ((b1 & 0x0F) << 8)
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4)
    return out


def main() -> None:
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    rng = np.random.default_rng(SEED)
    t_hat = rng.integers(0, Q, size=(K * N,), dtype=np.int32)
    packed = np.zeros(K * POLY_BYTES, dtype=np.uint8)
    for p in range(K):
        poly = t_hat[p * N : (p + 1) * N]
        enc = byte_encode12(poly)
        # round-trip 自检
        back = byte_decode12(enc)
        if not np.array_equal(back, poly):
            raise SystemExit(f"round-trip FAIL poly {p}")
        packed[p * POLY_BYTES : (p + 1) * POLY_BYTES] = enc

    packed.tofile(os.path.join(_CASE_DIR, "input", "ek_t_hat.bin"))
    t_hat.tofile(os.path.join(_CASE_DIR, "output", "golden_t_hat.bin"))
    print(f"[gen_data] k={K} in={K * POLY_BYTES}B out={K * N} coeffs max={int(t_hat.max())}")


if __name__ == "__main__":
    main()
