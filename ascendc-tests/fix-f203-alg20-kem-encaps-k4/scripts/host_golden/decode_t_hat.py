#!/usr/bin/env python3
"""
decode_t_hat.py — 从 ek_pke.bin 解出平面 t̂ 供 host gate 对照。

## 流水线位置
Encrypt/Encaps 分阶段验收：设备写出的中间 t̂（或 ek 内嵌 t̂）与本脚本
ByteDecode₁₂ 结果逐系数 max=0。

## I/O
输入 ek_pke（≥1536B）；输出 t_hat.bin 为 [K*N] int32 小端平面布局。
非设备规格，仅 oracle。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

K = 4
N = 256
POLY_D12_BYTES = 384
EK_T_BYTES = 1536


def poly_byte_decode12(buf: bytes) -> np.ndarray:
    """单 poly ByteDecode₁₂：384B → [256] int32（12-bit 零扩展）。"""
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
    """解 k 个 poly 的 t̂，拼成长度 K*N 的平面向量。"""
    # 按 poly 下标 j=0..k-1：每段 384B → N 个系数写入平面缓冲
    t_flat = np.empty(K * N, dtype=np.int32)
    for j in range(K):
        off = j * POLY_D12_BYTES
        t_flat[j * N : (j + 1) * N] = poly_byte_decode12(ek[off : off + POLY_D12_BYTES])
    return t_flat


def main() -> None:
    """CLI：ek_pke.bin → t_hat.bin（int32 LE）。"""
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
