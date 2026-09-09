#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_data — RB-D03 Decrypt L2b：造 ŵ/v 与 golden w/m。

Host oracle（FIPS 203 契约，非 liboqs 权威）：
  - ŵ：随机 [0,q) NTT 域单 poly（对齐 D02 输出布局 int32[256]）
  - v：随机 [0,q) 时域单 poly（对齐 D01 Decompress₅ 后布局）
  - w = InverseNTT(ŵ)（Alg.10）
  - m = ByteEncode₁(Compress₁(v − w))（Alg.15 尾；统一整数 d=1）
ζ：只读 ntt_onnx 表（via T10 topology_math）。
禁把最终 m 写入 input（设备 MIX 才算）。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts"))
from topology_math import (  # noqa: E402
    Q,
    N,
    load_zetas_gammas,
    mlkem_intt,
    mod_q,
)

SEED = 20260909
# Compress_1 统一整数：C=⌊2^37/q⌋；shift=36；bias=2^35（notes 统一整数总结）
COMPRESS_C = 41285357
COMPRESS1_SHIFT = 36
COMPRESS1_BIAS = 1 << 35


def compress1(u: int) -> int:
    """Compress_1：round(u·2/q) mod 2。"""
    s = COMPRESS_C * int(u) + COMPRESS1_BIAS
    return (s >> COMPRESS1_SHIFT) & 1


def byte_encode1(bits: np.ndarray) -> bytes:
    """ByteEncode_1：256×1bit → 32B（LSB first）。"""
    out = bytearray(32)
    for i in range(N):
        bit = int(bits[i]) & 1
        out[i // 8] |= bit << (i % 8)
    return bytes(out)


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(SEED)

    # ŵ：NTT 域；v：时域（与 D02/D01 写出语义一致）
    w_hat = rng.integers(0, Q, size=(N,), dtype=np.int32)
    v = rng.integers(0, Q, size=(N,), dtype=np.int32)

    zetas_list, _gammas = load_zetas_gammas()
    zetas = np.asarray(zetas_list, dtype=np.int32)

    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)

    w_hat.tofile(inp / "w_hat.bin")
    v.tofile(inp / "v.bin")
    zetas.tofile(inp / "zetas.bin")
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    # golden w：Alg.10
    w = np.asarray(mlkem_intt(w_hat.tolist()), dtype=np.int32)

    # golden m：Compress₁(v−w) → ByteEncode₁
    bits = np.zeros(N, dtype=np.int32)
    for i in range(N):
        diff = mod_q(int(v[i]) - int(w[i]))
        bits[i] = compress1(diff)
    m = byte_encode1(bits)

    w.tofile(out / "golden_w.bin")
    (out / "golden_m.bin").write_bytes(m)

    print(
        f"[gen_data] ŵ={w_hat.shape} v={v.shape} "
        f"w sum={int(w.sum())} m_hex={m.hex()[:16]}…"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
