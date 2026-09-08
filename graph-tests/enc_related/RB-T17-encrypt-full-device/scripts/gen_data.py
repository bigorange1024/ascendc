#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T17 gen_data：Host 仅 ek/coins/μ/t̂/ζ/γ；golden 用设备 CBD 同式 y/e + Â/ŷ/u/v/c。

契约：
  - coins：T07 FIXED_COINS；设备 CBD（禁写入 input 最终 y/e）
  - Â：16×Alg.7 SampleNTT(ρ)；ŷ：Alg.9 NTT(y)
  - e1/e2：与 CBD 同源（相对 T15 随机噪改为 coins CBD）
  - t̂/μ：Host 预喂；禁写最终 Â/ŷ/u/v/c 到 input
"""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_SCRIPT))
sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T07-prep-shell" / "scripts"))
sys.path.insert(0, str(_REPO / "library" / "shared" / "fips203_se_sample"))
from prep_host import (  # noqa: E402
    FIXED_COINS,
    FIXED_EK_HEAD,
    FIXED_RHO,
    extract_rho,
)
from golden_se_sampling import sample_poly_cbd2  # noqa: E402
from topology_math import (  # noqa: E402
    K,
    N,
    Q,
    compute_u_v,
    load_zetas_gammas,
    mlkem_ntt,
    mu_embed_from_m,
)

ALG7_SCRIPTS = (
    _REPO
    / "ascendc-tests"
    / "ml-kem"
    / "ml-kem-1024"
    / "pass-fix-f203-alg7-sample-ntt-k4"
    / "scripts"
)
sys.path.insert(0, str(ALG7_SCRIPTS))
from alg7_geom import XOF_BYTES  # noqa: E402
from gen_data import (  # noqa: E402
    rej_bulk_from_d12,
    rej_scalar_from_d12,
    shake128_squeeze,
    unpack_d12_from_xof,
)

SEED = 20260908
FIXED_M = bytes([(0xA5 if (i % 2 == 0) else 0x5A) for i in range(32)])
SEED_MAT = 42
C_LEN = 1568
AHAT_POLYS = K * K
PRF_OUT = 2 * N // 4  # 128


def prf_shake256(coins: bytes, nonce: int) -> bytes:
    return hashlib.shake_256(coins + bytes([nonce & 0xFF])).digest(PRF_OUT)


def sample_y_e1_e2(coins: bytes) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Alg.14 行 8–15：coins→y/e1/e2（η=2）；与 T16 设备 CBD 同式。"""
    nonce = 0
    y_rows = []
    for _ in range(K):
        y_rows.append(sample_poly_cbd2(prf_shake256(coins, nonce)))
        nonce += 1
    e1_rows = []
    for _ in range(K):
        e1_rows.append(sample_poly_cbd2(prf_shake256(coins, nonce)))
        nonce += 1
    e2 = sample_poly_cbd2(prf_shake256(coins, nonce))
    return np.stack(y_rows), np.stack(e1_rows), e2


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


def sample_one_poly(rho: bytes, p: int, j: int) -> np.ndarray:
    seed = rho + bytes([j & 0xFF, p & 0xFF])
    xof = shake128_squeeze(seed, XOF_BYTES)
    d1, d2 = unpack_d12_from_xof(xof)
    a_spec = rej_scalar_from_d12(d1, d2)
    a_bulk = rej_bulk_from_d12(d1, d2)
    if not np.array_equal(a_spec, a_bulk):
        raise SystemExit(f"spec vs bulk mismatch at p={p} j={j}")
    return a_spec


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    ek = FIXED_EK_HEAD + FIXED_RHO
    coins = FIXED_COINS
    rho = extract_rho(ek)
    y, e1, e2 = sample_y_e1_e2(coins)
    y_e1_e2 = np.concatenate(
        [y.reshape(-1), e1.reshape(-1), e2.reshape(-1)]
    ).astype(np.int32)

    a_hat_flat = np.empty(AHAT_POLYS * N, dtype=np.int32)
    for p in range(K):
        for j in range(K):
            poly = sample_one_poly(rho, p, j)
            off = (p * K + j) * N
            a_hat_flat[off : off + N] = poly
    a_hat = a_hat_flat.reshape(K, K, N)
    y_hat = np.stack([np.asarray(mlkem_ntt(y[i].tolist()), dtype=np.int32) for i in range(K)], axis=0)

    rng = np.random.default_rng(SEED)

    def rand_hat_poly() -> np.ndarray:
        time = rng.integers(0, Q, size=N, dtype=np.int32)
        return np.asarray(mlkem_ntt(time.tolist()), dtype=np.int32)

    t_hat = np.stack([rand_hat_poly() for _ in range(K)], axis=0)
    mu = mu_embed_from_m(FIXED_M)
    zetas_list, gammas_list = load_zetas_gammas()
    zetas = np.asarray(zetas_list, dtype=np.int32)
    gammas = np.asarray(gammas_list, dtype=np.int32)

    rng_mat = np.random.default_rng(SEED_MAT)
    mat_a = rng_mat.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng_mat.integers(-8, 9, size=(32, 32), dtype=np.int8)

    (inp / "ek.bin").write_bytes(ek)
    (inp / "coins.bin").write_bytes(coins)
    # 禁写 y_e1_e2 / e1 / e2 / a_hat / y_hat / u / v / c 到 input
    t_hat.astype(np.int32).tofile(inp / "t_hat.bin")
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
    a_hat_flat.tofile(out / "golden_a_hat.bin")
    y_hat.astype(np.int32).tofile(out / "golden_y_hat.bin")
    u.astype(np.int32).tofile(out / "golden_u.bin")
    v.astype(np.int32).tofile(out / "golden_v.bin")
    (out / "golden_c.bin").write_bytes(golden_c)

    print(
        f"[gen_data] T17 coins CBD y/e; device Â/ŷ golden; "
        f"u_sum={int(u.sum())} c={len(golden_c)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
