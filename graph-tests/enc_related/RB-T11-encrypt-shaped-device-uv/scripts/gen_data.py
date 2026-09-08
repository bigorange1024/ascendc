#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T11 gen_data：Launch1 prep 半桩输入 + Launch2 拓扑预喂 + golden u/v/c。

契约：
  - prep：T07 Host 公式（ρ / y‖e1‖e2）；禁抄 encrypt
  - 拓扑：同 T10 — Host 预生成 Â/ŷ/t̂/e₁/e₂/μ/ζ/γ；**不**把最终 u,v 写入 input
  - golden_u/v：topology_math.compute_u_v（黑盒 oracle）
  - golden_c：Compress₁₁/₅ + ByteEncode（与 T06 同式，基于 golden u,v）
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_SCRIPT))
sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T07-prep-shell" / "scripts"))
from prep_host import (  # noqa: E402
    FIXED_COINS,
    FIXED_EK_HEAD,
    FIXED_RHO,
    extract_rho,
    sample_y_e1_e2,
)
from topology_math import (  # noqa: E402
    K,
    N,
    Q,
    compute_u_v,
    load_zetas_gammas,
    mlkem_ntt,
    mu_embed_from_m,
)

SEED = 20260908
FIXED_M = bytes([(0xA5 if (i % 2 == 0) else 0x5A) for i in range(32)])
SEED_MAT = 42
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
        out[base + 10] = (t[7] >> 3) & 0xFF
    return bytes(out)


def pack_c(u: np.ndarray, v: np.ndarray) -> bytes:
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

    # ---- Launch1 prep 半桩 ----
    ek = FIXED_EK_HEAD + FIXED_RHO
    coins = FIXED_COINS
    rho = extract_rho(ek)
    y, e1_prep, e2_prep = sample_y_e1_e2(coins)
    y_e1_e2 = np.concatenate(
        [y.reshape(-1), e1_prep.reshape(-1), e2_prep.reshape(-1)]
    ).astype(np.int32)

    # ---- Launch2 拓扑预喂（同 T10；禁写最终 u,v 到 input）----
    rng = np.random.default_rng(SEED)

    def rand_hat_poly() -> np.ndarray:
        time = rng.integers(0, Q, size=N, dtype=np.int32)
        return np.asarray(mlkem_ntt(time.tolist()), dtype=np.int32)

    a_hat = np.stack([[rand_hat_poly() for _ in range(K)] for _ in range(K)], axis=0)
    y_hat = np.stack([rand_hat_poly() for _ in range(K)], axis=0)
    t_hat = np.stack([rand_hat_poly() for _ in range(K)], axis=0)
    e1 = np.mod(rng.integers(-2, 3, size=(K, N), dtype=np.int32), Q).astype(np.int32)
    e2 = np.mod(rng.integers(-2, 3, size=(N,), dtype=np.int32), Q).astype(np.int32)
    mu = mu_embed_from_m(FIXED_M)
    zetas_list, gammas_list = load_zetas_gammas()
    zetas = np.asarray(zetas_list, dtype=np.int32)
    gammas = np.asarray(gammas_list, dtype=np.int32)

    rng_mat = np.random.default_rng(SEED_MAT)
    mat_a = rng_mat.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng_mat.integers(-8, 9, size=(32, 32), dtype=np.int8)

    (inp / "ek.bin").write_bytes(ek)
    (inp / "coins.bin").write_bytes(coins)
    y_e1_e2.tofile(inp / "y_e1_e2.bin")
    a_hat.astype(np.int32).tofile(inp / "a_hat.bin")
    y_hat.astype(np.int32).tofile(inp / "y_hat.bin")
    t_hat.astype(np.int32).tofile(inp / "t_hat.bin")
    e1.astype(np.int32).tofile(inp / "e1.bin")
    e2.astype(np.int32).tofile(inp / "e2.bin")
    mu.astype(np.int32).tofile(inp / "mu.bin")
    zetas.tofile(inp / "zetas.bin")
    gammas.tofile(inp / "gammas.bin")
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")
    (inp / "m.bin").write_bytes(FIXED_M)

    u, v = compute_u_v(a_hat, y_hat, t_hat, e1, e2, mu)
    golden_c = pack_c(u, v)

    (out / "golden_rho.bin").write_bytes(rho)
    y_e1_e2.tofile(out / "golden_y_e1_e2.bin")
    u.astype(np.int32).tofile(out / "golden_u.bin")
    v.astype(np.int32).tofile(out / "golden_v.bin")
    (out / "golden_c.bin").write_bytes(golden_c)

    print(f"[gen_data] prep y_e1_e2={y_e1_e2.shape} rho={len(rho)}")
    print(
        f"[gen_data] Â={a_hat.shape} ŷ={y_hat.shape} t̂={t_hat.shape} "
        f"golden u sum={int(u.sum())} c={len(golden_c)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
