#!/usr/bin/env python3
"""
golden_m.py — Alg.15 全链 host golden：dk,c → m.bin（32B）。

禁止 liboqs；与设备 G1–G4 语义对齐。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from f203_ref_common import (
    K,
    N,
    Q,
    mod_q_i64,
    multiply_ntts,
    stage123_transform,
)

DK_BYTES = 1536
CT_BYTES = 1568
MSG_BYTES = 32
C1_BYTES = 1408
C1_POLY_BYTES = 352
C2_BYTES = 160


def poly_byte_decode12(buf: bytes) -> np.ndarray:
    out = np.empty(N, dtype=np.int32)
    for i in range(N // 2):
        b0, b1, b2 = buf[3 * i], buf[3 * i + 1], buf[3 * i + 2]
        out[2 * i] = b0 | ((b1 & 0x0F) << 8)
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4)
    return out


def decode_s_hat(dk: bytes) -> np.ndarray:
    s = np.empty((K, N), dtype=np.int32)
    for j in range(K):
        s[j] = poly_byte_decode12(dk[j * 384 : (j + 1) * 384])
    return s


def byte_decode_d(bits_src: bytes, d: int) -> np.ndarray:
    out = np.empty(N, dtype=np.int32)
    bit_pos = 0
    mask = (1 << d) - 1
    for i in range(N):
        a = 0
        for j in range(d):
            byte_idx = bit_pos >> 3
            bit_idx = bit_pos & 7
            if (bits_src[byte_idx] >> bit_idx) & 1:
                a |= 1 << j
            bit_pos += 1
        out[i] = a & mask
    return out


def decompress_d_scalar(u: int, d: int) -> int:
    u = int(u)
    if d == 11:
        return (u * Q + 1024) >> 11
    if d == 5:
        return (u * Q + 16) >> 5
    if d == 10:
        return (u * Q + 512) >> 10
    if d == 4:
        return (u * Q + 8) >> 4
    raise ValueError(f"d={d}")


def unpack_ciphertext(c: bytes) -> tuple[np.ndarray, np.ndarray]:
    u = np.empty((K, N), dtype=np.int32)
    for p in range(K):
        chunk = c[p * C1_POLY_BYTES : (p + 1) * C1_POLY_BYTES]
        comp = byte_decode_d(chunk, 11)
        u[p] = np.array([decompress_d_scalar(int(x), 11) for x in comp], dtype=np.int32)
    comp_v = byte_decode_d(c[C1_BYTES : C1_BYTES + C2_BYTES], 5)
    v = np.array([decompress_d_scalar(int(x), 5) for x in comp_v], dtype=np.int32)
    return u, v


def compress_1_scalar(x: int) -> int:
    x = mod_q_i64(int(x))
    half = (Q + 1) // 2
    return ((x << 1) + half) // Q & 1


def extract_message(w: np.ndarray) -> bytes:
    msg = bytearray(MSG_BYTES)
    for i in range(N):
        bit = compress_1_scalar(int(w[i]))
        if bit:
            msg[i >> 3] |= 1 << (i & 7)
    return bytes(msg)


def golden_w_hat(s_hat: np.ndarray, u_hat: np.ndarray) -> np.ndarray:
    acc = np.zeros(N, dtype=np.int64)
    for j in range(K):
        prod = multiply_ntts(s_hat[j], u_hat[j])
        acc += prod.astype(np.int64)
    return np.array([mod_q_i64(int(v)) for v in acc], dtype=np.int32)


def golden_decrypt(dk: bytes, c: bytes) -> bytes:
    s_hat = decode_s_hat(dk)
    u, v = unpack_ciphertext(c)
    u_hat = stage123_transform(u, "ntt")
    w_hat = golden_w_hat(s_hat, u_hat)
    w_pad = np.zeros((K, N), dtype=np.int32)
    w_pad[0] = w_hat
    w_time = stage123_transform(w_pad, "intt")[0]
    w = np.array([mod_q_i64(int(v[i]) - int(w_time[i])) for i in range(N)], dtype=np.int32)
    return extract_message(w)


def main() -> None:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} <dk_pke> <c.bin> <golden_m.out>", file=sys.stderr)
        sys.exit(1)
    dk = Path(sys.argv[1]).read_bytes()
    c = Path(sys.argv[2]).read_bytes()
    out_path = Path(sys.argv[3])
    if len(dk) != DK_BYTES or len(c) != CT_BYTES:
        raise SystemExit(f"bad sizes dk={len(dk)} c={len(c)}")
    m = golden_decrypt(dk, c)
    if len(m) != MSG_BYTES:
        raise SystemExit(f"golden m size {len(m)} != {MSG_BYTES}")
    out_path.write_bytes(m)
    print(f"[golden_m] OK {len(m)}B")


if __name__ == "__main__":
    main()
