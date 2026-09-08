#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T18 gen_data：Host 预喂完整 ek（BE₁₂(t̂)‖ρ）+ golden t̂。

契约（对齐 T05 / Alg.14 行 2；本刀不抄 T05 源码）：
  - ek[1568] = packed_t̂[1536] ‖ ρ[32]
  - golden：随机 t̂∈[0,q)，经 BE₁₂ round-trip 自检
  - **不**把最终 t̂ 写入 input（设备 MIX 才算）
"""
from __future__ import annotations

from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent

K = 4
N = 256
Q = 3329
POLY_BYTES = 384
EK_BODY = K * POLY_BYTES  # 1536
RHO_BYTES = 32
EK_BYTES = EK_BODY + RHO_BYTES  # 1568
SEED = 20260918


def byte_encode12(coeffs: np.ndarray) -> np.ndarray:
    """coeffs int32[256] → uint8[384]（FIPS Alg.5 d=12）。"""
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
    """uint8[384] → int32[256]（golden 自洽校验）。"""
    assert buf.shape == (POLY_BYTES,)
    out = np.zeros(N, dtype=np.int32)
    for i in range(N // 2):
        b0 = int(buf[3 * i + 0])
        b1 = int(buf[3 * i + 1])
        b2 = int(buf[3 * i + 2])
        out[2 * i] = b0 | ((b1 & 0x0F) << 8)
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4)
    return out


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(SEED)
    t_hat = rng.integers(0, Q, size=(K * N,), dtype=np.int32)
    packed = np.zeros(EK_BODY, dtype=np.uint8)
    for p in range(K):
        poly = t_hat[p * N : (p + 1) * N]
        enc = byte_encode12(poly)
        back = byte_decode12(enc)
        if not np.array_equal(back, poly):
            raise SystemExit(f"round-trip FAIL poly {p}")
        packed[p * POLY_BYTES : (p + 1) * POLY_BYTES] = enc

    rho = rng.integers(0, 256, size=(RHO_BYTES,), dtype=np.uint8)
    ek = np.concatenate([packed, rho])
    assert ek.size == EK_BYTES
    ek.tofile(inp / "ek.bin")
    # 另落 ek 体，便于对照 T05 形状（设备仍读完整 ek）
    packed.tofile(inp / "ek_t_hat.bin")

    t_hat.tofile(out / "golden_t_hat.bin")

    # 极轻 Cube 输入（非零即可）
    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    print(
        f"[gen_data] ek={EK_BYTES}B body={EK_BODY}B rho={RHO_BYTES}B "
        f"t_hat_max={int(t_hat.max())} coeffs={K * N}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
