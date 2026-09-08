#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""RB-T01：生成极轻 Cube 输入 A/B（全 1 / 近似单位块）。无算法 golden。"""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
INP = ROOT / "input"
INP.mkdir(parents=True, exist_ok=True)

M, K, N = 16, 32, 32
# A 全 1；B 对角线 1（其余 0）→ C 行方向为 1（冒烟可查，非验收硬条件）
a = bytes([1] * (M * K))
b = bytearray(K * N)
for i in range(min(K, N)):
    b[i * N + i] = 1

(INP / "mat_a.bin").write_bytes(a)
(INP / "mat_b.bin").write_bytes(bytes(b))
(INP / "tiling.bin").write_bytes(b"\x00" * 64)
print(f"[gen_data] mat_a={len(a)} mat_b={len(b)} → {INP}")
