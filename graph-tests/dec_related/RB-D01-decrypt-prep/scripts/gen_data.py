#!/usr/bin/env python3
"""gen_data — RB-D01 Decrypt prep：造 dk_pke/c 与 golden ŝ/u/v。

Host oracle（FIPS 203 契约，非 liboqs 权威）：
  - ŝ：随机 [0,q) → ByteEncode₁₂ → dk_pke[1536]；golden=原 ŝ
  - u：随机压缩域 [0,2^11) → ByteEncode₁₁ → c₁；golden=Decompress₁₁
  - v：随机压缩域 [0,2^5)  → ByteEncode₅  → c₂；golden=Decompress₅
c = c₁‖c₂（1408+160=1568）。
"""
from __future__ import annotations

import os

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))

K = 4
N = 256
Q = 3329
D_U = 11
D_V = 5
POLY_BYTES_12 = 384
POLY_BYTES_11 = 352
POLY_BYTES_5 = 160
SEED = 20260909


def byte_encode12(coeffs: np.ndarray) -> np.ndarray:
    """coeffs int32[256] → uint8[384]。"""
    out = np.zeros(POLY_BYTES_12, dtype=np.uint8)
    for i in range(N // 2):
        v0 = int(coeffs[2 * i]) & 0xFFF
        v1 = int(coeffs[2 * i + 1]) & 0xFFF
        out[3 * i + 0] = v0 & 0xFF
        out[3 * i + 1] = ((v0 >> 8) & 0x0F) | ((v1 & 0x0F) << 4)
        out[3 * i + 2] = (v1 >> 4) & 0xFF
    return out


def byte_encode5(coeffs: np.ndarray) -> np.ndarray:
    """压缩域 int32[256] → uint8[160]（Alg.5 d=5）。"""
    out = np.zeros(POLY_BYTES_5, dtype=np.uint8)
    for g in range(N // 8):
        t = [int(coeffs[g * 8 + j]) & 0x1F for j in range(8)]
        b = g * 5
        out[b + 0] = (t[0] | (t[1] << 5)) & 0xFF
        out[b + 1] = ((t[1] >> 3) | (t[2] << 2) | (t[3] << 7)) & 0xFF
        out[b + 2] = ((t[3] >> 1) | (t[4] << 4)) & 0xFF
        out[b + 3] = ((t[4] >> 4) | (t[5] << 1) | (t[6] << 6)) & 0xFF
        out[b + 4] = ((t[6] >> 2) | (t[7] << 3)) & 0xFF
    return out


def byte_encode11(coeffs: np.ndarray) -> np.ndarray:
    """压缩域 int32[256] → uint8[352]（Alg.5 d=11）。"""
    out = np.zeros(POLY_BYTES_11, dtype=np.uint8)
    for g in range(N // 8):
        t = [int(coeffs[g * 8 + j]) & 0x7FF for j in range(8)]
        b = g * 11
        out[b + 0] = t[0] & 0xFF
        out[b + 1] = ((t[0] >> 8) | (t[1] << 3)) & 0xFF
        out[b + 2] = ((t[1] >> 5) | (t[2] << 6)) & 0xFF
        out[b + 3] = (t[2] >> 2) & 0xFF
        out[b + 4] = ((t[2] >> 10) | (t[3] << 1)) & 0xFF
        out[b + 5] = ((t[3] >> 7) | (t[4] << 4)) & 0xFF
        out[b + 6] = ((t[4] >> 4) | (t[5] << 7)) & 0xFF
        out[b + 7] = (t[5] >> 1) & 0xFF
        out[b + 8] = ((t[5] >> 9) | (t[6] << 2)) & 0xFF
        out[b + 9] = ((t[6] >> 6) | (t[7] << 5)) & 0xFF
        out[b + 10] = (t[7] >> 3) & 0xFF
    return out


def decompress(comp: np.ndarray, d: int) -> np.ndarray:
    """Decompress_d：((c*q + 2^(d-1)) >> d)。"""
    bias = 1 << (d - 1)
    return ((comp.astype(np.int64) * Q + bias) >> d).astype(np.int32)


def main() -> None:
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    rng = np.random.default_rng(SEED)

    # ŝ：无损 BD₁₂ round-trip
    s_hat = rng.integers(0, Q, size=(K * N,), dtype=np.int32)
    dk = np.zeros(K * POLY_BYTES_12, dtype=np.uint8)
    for p in range(K):
        dk[p * POLY_BYTES_12 : (p + 1) * POLY_BYTES_12] = byte_encode12(
            s_hat[p * N : (p + 1) * N]
        )

    # u：压缩域 → encode → golden=decompress
    u_comp = rng.integers(0, 1 << D_U, size=(K * N,), dtype=np.int32)
    c_u = np.zeros(K * POLY_BYTES_11, dtype=np.uint8)
    for p in range(K):
        c_u[p * POLY_BYTES_11 : (p + 1) * POLY_BYTES_11] = byte_encode11(
            u_comp[p * N : (p + 1) * N]
        )
    u_golden = decompress(u_comp, D_U)

    # v
    v_comp = rng.integers(0, 1 << D_V, size=(N,), dtype=np.int32)
    c_v = byte_encode5(v_comp)
    v_golden = decompress(v_comp, D_V)

    c = np.concatenate([c_u, c_v])
    assert c.shape[0] == 1568

    dk.tofile(os.path.join(_CASE_DIR, "input", "dk_pke.bin"))
    c.tofile(os.path.join(_CASE_DIR, "input", "c.bin"))
    s_hat.tofile(os.path.join(_CASE_DIR, "output", "golden_s_hat.bin"))
    u_golden.tofile(os.path.join(_CASE_DIR, "output", "golden_u.bin"))
    v_golden.tofile(os.path.join(_CASE_DIR, "output", "golden_v.bin"))
    print(
        f"[gen_data] dk={dk.size}B c={c.size}B "
        f"ŝ={s_hat.size} u={u_golden.size} v={v_golden.size} "
        f"(host FIPS oracle, NOT liboqs)"
    )


if __name__ == "__main__":
    main()
