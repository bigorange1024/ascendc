#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T06 gen_data：预生成域元素 u[k·256]、v[256]，并计算 golden c[1568]。

Compress/ByteEncode 公式与 pass-f203-compress-d / byteencode-d 的 ref 同式（黑盒 oracle）；
设备实现不要求同构，只验 I/O。
"""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
INP = ROOT / "input"
OUT = ROOT / "output"
INP.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)

K, N, Q = 4, 256, 3329
SEED = 20260908
C1_OFF, C1_LEN = 0, 1408
C2_OFF, C2_LEN = 1408, 160
C_LEN = 1568


def compress5(u: int) -> int:
    x = u if u < Q else Q - 1
    d0 = (x * 1290176) & 0xFFFFFFFF
    return ((d0 + (1 << 26)) >> 27) & 0x1F


def compress11(u: int) -> int:
    x = u if u < Q else Q - 1
    d0 = x * 5284526080
    d0 = (d0 + (1 << 32)) >> 33
    return d0 & 0x7FF


def byte_encode5(comp: list[int]) -> bytes:
    out = bytearray(N * 5 // 8)
    for i in range(N // 8):
        t = [comp[8 * i + j] & 0x1F for j in range(8)]
        base = i * 5
        out[base + 0] = 0xFF & ((t[0] >> 0) | (t[1] << 5))
        out[base + 1] = 0xFF & ((t[1] >> 3) | (t[2] << 2) | (t[3] << 7))
        out[base + 2] = 0xFF & ((t[3] >> 1) | (t[4] << 4))
        out[base + 3] = 0xFF & ((t[4] >> 4) | (t[5] << 1) | (t[6] << 6))
        out[base + 4] = 0xFF & ((t[6] >> 2) | (t[7] << 3))
    return bytes(out)


def byte_encode11(comp: list[int]) -> bytes:
    out = bytearray(N * 11 // 8)
    for j in range(N // 8):
        t = [comp[8 * j + k] & 0x7FF for k in range(8)]
        base = 11 * j
        out[base + 0] = (t[0] >> 0) & 0xFF
        out[base + 1] = (t[0] >> 8) | ((t[1] << 3) & 0xFF)
        out[base + 2] = (t[1] >> 5) | ((t[2] << 6) & 0xFF)
        out[base + 3] = (t[2] >> 2) & 0xFF
        out[base + 4] = (t[2] >> 10) | ((t[3] << 1) & 0xFF)
        out[base + 5] = (t[3] >> 7) | ((t[4] << 4) & 0xFF)
        out[base + 6] = (t[4] >> 4) | ((t[5] << 7) & 0xFF)
        out[base + 7] = (t[5] >> 1) & 0xFF
        out[base + 8] = (t[5] >> 9) | ((t[6] << 2) & 0xFF)
        out[base + 9] = (t[6] >> 6) | ((t[7] << 5) & 0xFF)
        out[base + 10] = t[7] >> 3
    return bytes(out)


def main() -> None:
    # 固定可复现输入：u/v ∈ [0, q)
    rng_state = SEED
    def rnd() -> int:
        nonlocal rng_state
        rng_state = (1103515245 * rng_state + 12345) & 0x7FFFFFFF
        return rng_state % Q

    u = [rnd() for _ in range(K * N)]
    v = [rnd() for _ in range(N)]

    c1 = bytearray()
    for p in range(K):
        poly = u[p * N : (p + 1) * N]
        comp = [compress11(x) for x in poly]
        c1.extend(byte_encode11(comp))
    assert len(c1) == C1_LEN

    c2 = byte_encode5([compress5(x) for x in v])
    assert len(c2) == C2_LEN

    c = bytes(c1) + c2
    assert len(c) == C_LEN

    (INP / "u.bin").write_bytes(struct.pack(f"<{K*N}i", *u))
    (INP / "v.bin").write_bytes(struct.pack(f"<{N}i", *v))
    (OUT / "golden_c.bin").write_bytes(c)

    print(
        f"[gen_data] u={K*N} v={N} c={C_LEN} "
        f"c1=[{C1_OFF},{C1_OFF+C1_LEN}) c2=[{C2_OFF},{C2_OFF+C2_LEN})"
    )


if __name__ == "__main__":
    main()
