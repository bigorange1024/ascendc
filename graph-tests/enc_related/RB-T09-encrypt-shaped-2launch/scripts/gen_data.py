#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T09 gen_data：Host 预算 T07 prep + T08 半桩 u,v + T06 golden c + 极轻 Cube 输入。

契约：
  - ρ/y/e1/e2：复用 T07 prep_host 公式（shared CBD；禁抄 encrypt）
  - u,v：预喂「拓扑产出」半桩（固定种子 [0,q)），设备侧不真算 Âᵀ∘ŷ
  - golden_c：Compress₁₁/₅ + ByteEncode（与 T06 同式 oracle）
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

# 复用 T07 Host 公式（只读 import；不改 T07 树）
sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T07-prep-shell" / "scripts"))
from prep_host import (  # noqa: E402
    FIXED_COINS,
    FIXED_EK_HEAD,
    FIXED_RHO,
    extract_rho,
    sample_y_e1_e2,
)

K, N, Q = 4, 256, 3329
SEED_UV = 20260909
SEED_MAT = 42
C1_LEN, C2_LEN, C_LEN = 1408, 160, 1568


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
        out[base + 10] = (t[7] >> 3) & 0xFF
    return bytes(out)


def pack_c(u: np.ndarray, v: np.ndarray) -> bytes:
    """u[4,256]+v[256] → c[1568]。"""
    parts = []
    for p in range(K):
        parts.append(byte_encode11([compress11(int(x)) for x in u[p].tolist()]))
    parts.append(byte_encode5([compress5(int(x)) for x in v.tolist()]))
    c = b"".join(parts)
    assert len(c) == C_LEN
    return c


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    ek = FIXED_EK_HEAD + FIXED_RHO
    coins = FIXED_COINS
    rho = extract_rho(ek)
    y, e1, e2 = sample_y_e1_e2(coins)
    y_e1_e2 = np.concatenate([y.reshape(-1), e1.reshape(-1), e2.reshape(-1)]).astype(np.int32)

    rng_uv = np.random.default_rng(SEED_UV)
    u = rng_uv.integers(0, Q, size=(K, N), dtype=np.int32)
    v = rng_uv.integers(0, Q, size=(N,), dtype=np.int32)
    golden_c = pack_c(u, v)

    rng_mat = np.random.default_rng(SEED_MAT)
    mat_a = rng_mat.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng_mat.integers(-8, 9, size=(32, 32), dtype=np.int8)

    (inp / "ek.bin").write_bytes(ek)
    (inp / "coins.bin").write_bytes(coins)
    y_e1_e2.tofile(inp / "y_e1_e2.bin")
    u.astype(np.int32).tofile(inp / "u.bin")
    v.astype(np.int32).tofile(inp / "v.bin")
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    (out / "golden_rho.bin").write_bytes(rho)
    y_e1_e2.tofile(out / "golden_y_e1_e2.bin")
    u.astype(np.int32).tofile(out / "golden_u.bin")
    v.astype(np.int32).tofile(out / "golden_v.bin")
    (out / "golden_c.bin").write_bytes(golden_c)

    print(f"[gen_data] ek={len(ek)} coins={len(coins)} y_e1_e2={y_e1_e2.shape}")
    print(f"[gen_data] u={u.shape} v={v.shape} golden_c={len(golden_c)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
