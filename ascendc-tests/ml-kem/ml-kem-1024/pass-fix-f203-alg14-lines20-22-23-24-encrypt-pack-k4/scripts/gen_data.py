#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自包含 golden：m/u/v → mu_embed + c（Alg.14 行 20/22–24，ml_kem_1024）。

流水线位置：pack 探针 host 基准生成；公式对齐 FIPS 203 / correctness pack。
golden I/O：
  输入 input/{m,u,v}.bin；期望 golden/{mu_embed,c}.bin
  不 import 其它探针；仅提供 I/O 等价 oracle，非 AscendC 实现规格。

种子：TAIL_PACK_SEED（默认 20260708）。
"""
from __future__ import annotations

import os
import struct
from pathlib import Path

import numpy as np

Q = 3329
N = 256
K = 4
HALF_Q = (Q + 1) // 2
MSG_BYTES = 32
C1_POLY_BYTES = 352
C1_BYTES = K * C1_POLY_BYTES
C2_BYTES = 160
C_BYTES = C1_BYTES + C2_BYTES
SEED = int(os.environ.get("TAIL_PACK_SEED", "20260708"))


def mod_q_i32(x: int) -> int:
    """将整数规范到 [0, q)。"""
    x = int(x) % Q
    return x if x >= 0 else x + Q


def compress_d_scalar(u: int, d: int) -> int:
    """
    标量 Compress_d（与设备 Barrett/cast_div 路径 I/O 等价）。
    d=5：固定点 Barrett；d=11：大整数 round 后 mask 11 bit。
    """
    u = int(u) % Q
    if d == 5:
        d0 = u * 1290176
        return ((d0 + (1 << 26)) >> 27) & 0x1F
    if d == 11:
        d0 = u * 5284526080
        d0 = (d0 + (1 << 32)) >> 33
        return int(d0 & 0x7FF)
    raise ValueError(f"unsupported d={d}")


def byte_encode_d(F: np.ndarray, d: int) -> bytes:
    """
    Alg.5 ByteEncode_d：逐系数低 d bit 拼比特流再打包为字节。
    @param F  压缩后系数向量
    @param d  每系数比特数（5 或 11）
    """
    bits: list[int] = []
    mask = (1 << d) - 1
    for val in F:
        a = int(val) & mask
        for j in range(d):
            bits.append(a & 1)
            a >>= 1
    out = bytearray((len(bits) + 7) // 8)
    for i, b in enumerate(bits):
        if b:
            out[i >> 3] |= 1 << (i & 7)
    return bytes(out)


def mu_embed_from_m(m: bytes) -> np.ndarray:
    """行 20：仅展开 μ（⌊(q+1)/2⌋·bit），不加到 v。"""
    out = np.zeros(N, dtype=np.int32)
    for c in range(N):
        i, j = c // 8, c % 8
        bit = (m[i] >> j) & 1
        out[c] = HALF_Q if bit else 0
    return out


def pack_ciphertext(u: np.ndarray, v: np.ndarray) -> bytes:
    """
    行 22–24：c = ByteEncode₁₁(Compress₁₁(u)) ‖ ByteEncode₅(Compress₅(v))。
    @param u  shape [K,N] int32
    @param v  shape [N] int32
    @return   1568 字节密文
    """
    c1 = bytearray(C1_BYTES)
    for p in range(K):
        comp = np.array([compress_d_scalar(int(x), 11) for x in u[p]], dtype=np.int32)
        c1[p * C1_POLY_BYTES : (p + 1) * C1_POLY_BYTES] = byte_encode_d(comp, 11)
    comp_v = np.array([compress_d_scalar(int(x), 5) for x in v], dtype=np.int32)
    c2 = byte_encode_d(comp_v, 5)
    return bytes(c1) + c2


def write_bin(path: Path, data: bytes) -> None:
    """写二进制；必要时创建父目录。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main() -> None:
    """生成随机 m/u/v 与对应 golden mu_embed/c。"""
    rng = np.random.default_rng(SEED)
    root = Path(__file__).resolve().parents[1]
    inp = root / "input"
    gold = root / "golden"

    m = rng.integers(0, 256, size=MSG_BYTES, dtype=np.uint8).tobytes()
    u = rng.integers(0, Q, size=(K, N), dtype=np.int32)
    v = rng.integers(0, Q, size=N, dtype=np.int32)

    mu = mu_embed_from_m(m)
    c = pack_ciphertext(u, v)

    write_bin(inp / "m.bin", m)
    write_bin(inp / "u.bin", u.astype(np.int32).tobytes())
    write_bin(inp / "v.bin", v.astype(np.int32).tobytes())
    write_bin(gold / "mu_embed.bin", mu.astype(np.int32).tobytes())
    write_bin(gold / "c.bin", c)

    print(f"[gen_data] seed={SEED} m={MSG_BYTES}B u={K}x{N} v={N} c={C_BYTES}B")


if __name__ == "__main__":
    main()
