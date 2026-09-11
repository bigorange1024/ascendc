#!/usr/bin/env python3
"""gen_data — RB-T04 μ_embed：固定 m[32] → golden μ[256]（Decompress₁∘ByteDecode₁）。

本脚本是黑盒 oracle，不是 AscendC 实现规格。对齐 FIPS 203：
  bit_i = (m[i//8] >> (i%8)) & 1
  μ_i = (bit_i * 3329 + 1) >> 1   # ∈ {0, 1665}
"""
from __future__ import annotations

import os

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))

N = 256
MSG_BYTES = 32
Q = 3329
# 固定可复现消息：交替 0xA5 / 0x5A，覆盖 0/1 位混合
FIXED_M = bytes([(0xA5 if (i % 2 == 0) else 0x5A) for i in range(MSG_BYTES)])


def mu_embed_ref(m: bytes) -> np.ndarray:
    """Host golden：ByteDecode₁ + Decompress₁ → int32[256]。"""
    assert len(m) == MSG_BYTES
    out = np.zeros(N, dtype=np.int32)
    for i in range(N):
        bit = (m[i // 8] >> (i % 8)) & 1
        out[i] = (bit * Q + 1) >> 1
    return out


def main() -> None:
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    m = FIXED_M
    golden = mu_embed_ref(m)

    with open(os.path.join(_CASE_DIR, "input", "m.bin"), "wb") as f:
        f.write(m)
    golden.tofile(os.path.join(_CASE_DIR, "output", "golden_mu.bin"))

    ones = int(np.count_nonzero(golden))
    print(f"[gen_data] m={MSG_BYTES}B μ={N} ones={ones} zero={N - ones} max={int(golden.max())}")


if __name__ == "__main__":
    main()
