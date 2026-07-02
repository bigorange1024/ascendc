#!/usr/bin/env python3
"""Host 辅助：自 ek_pke 前 1536B ByteDecode₁₂ 得 t_hat[4,256]，供 G3 device 读 input/t_hat.bin。"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

K = 4
N = 256
POLY_D12_BYTES = 384
EK_T_BYTES = 1536


def poly_byte_decode12(buf: bytes) -> np.ndarray:
    out = np.empty(N, dtype=np.int32)
    pairs = N // 2
    for i in range(pairs):
        b0, b1, b2 = buf[3 * i], buf[3 * i + 1], buf[3 * i + 2]
        t0 = b0 | ((b1 & 0x0F) << 8)
        t1 = (b1 >> 4) | (b2 << 4)
        out[2 * i] = t0
        out[2 * i + 1] = t1
    return out


def decode_t_hat_polyvec(ek: bytes) -> np.ndarray:
    t_flat = np.empty(K * N, dtype=np.int32)
    for j in range(K):
        off = j * POLY_D12_BYTES
        t_flat[j * N : (j + 1) * N] = poly_byte_decode12(ek[off : off + POLY_D12_BYTES])
    return t_flat


def main() -> None:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <ek_pke.bin> <t_hat.out>", file=sys.stderr)
        sys.exit(1)
    ek = Path(sys.argv[1]).read_bytes()
    if len(ek) < EK_T_BYTES:
        raise SystemExit(f"ek too short: {len(ek)}")
    t_hat = decode_t_hat_polyvec(ek[:EK_T_BYTES])
    Path(sys.argv[2]).write_bytes(t_hat.tobytes())
    print(f"[decode_t_hat] OK {t_hat.nbytes}B")


if __name__ == "__main__":
    main()
