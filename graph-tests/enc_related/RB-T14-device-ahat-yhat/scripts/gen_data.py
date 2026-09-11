#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T14 gen_data：Host 预喂 ρ + y（CBD/coins）+ ζ；golden_Â / golden_ŷ。

契约：
  - ρ：T07 FIXED_RHO（ek 尾语义）
  - y：coins→PRF→SamplePolyCBD_η=2（同 T07/T12）
  - Â golden：16×Alg.7 SampleNTT（alg7_geom 黑盒）
  - ŷ golden：Alg.9（mlkem_ntt 黑盒）
  - **不**把最终 Â/ŷ 写入 input
"""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_REPO / "library" / "shared" / "fips203_se_sample"))
from golden_se_sampling import sample_poly_cbd2  # noqa: E402

sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts"))
from topology_math import load_zetas_gammas, mlkem_ntt  # noqa: E402

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

K = 4
N = 256
ETA = 2
RHO_BYTES = 32
COINS_BYTES = 32
PRF_OUT = ETA * N // 4  # 128
AHAT_POLYS = K * K

FIXED_RHO = bytes([(0x50 + (i * 7)) & 0xFF for i in range(RHO_BYTES)])
FIXED_COINS = bytes([(0xC0 ^ i) & 0xFF for i in range(COINS_BYTES)])


def prf_shake256(coins: bytes, nonce: int) -> bytes:
    """FIPS 203 PRF_η：SHAKE256(coins‖byte(N), 64·η)。"""
    assert len(coins) == COINS_BYTES
    return hashlib.shake_256(coins + bytes([nonce & 0xFF])).digest(PRF_OUT)


def sample_y(coins: bytes) -> np.ndarray:
    """coins → y[k,256]（η=2；nonce 0..3）。"""
    rows = []
    for nonce in range(K):
        rows.append(sample_poly_cbd2(prf_shake256(coins, nonce)))
    return np.stack(rows).astype(np.int32)


def a_hat_offset(p: int, j: int) -> int:
    """行主序：Â[p,j] 扁平起始下标。"""
    return (p * K + j) * N


def sample_one_poly(rho: bytes, p: int, j: int) -> np.ndarray:
    """Alg.7：seed=ρ‖byte(j)‖byte(i=p) → â[256]。"""
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

    rho = FIXED_RHO
    coins = FIXED_COINS
    (inp / "rho.bin").write_bytes(rho)
    (inp / "coins.bin").write_bytes(coins)

    y = sample_y(coins)
    zetas_list, _gammas = load_zetas_gammas()
    zetas = np.asarray(zetas_list, dtype=np.int32)

    a_hat = np.empty(AHAT_POLYS * N, dtype=np.int32)
    for p in range(K):
        for j in range(K):
            poly = sample_one_poly(rho, p, j)
            off = a_hat_offset(p, j)
            a_hat[off : off + N] = poly

    y_hat = np.stack([np.asarray(mlkem_ntt(y[i].tolist()), dtype=np.int32) for i in range(K)], axis=0)

    rng = np.random.default_rng(20260908)
    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)

    y.tofile(inp / "y.bin")
    zetas.tofile(inp / "zetas.bin")
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    a_hat.tofile(out / "golden_a_hat.bin")
    y_hat.tofile(out / "golden_y_hat.bin")
    (out / "golden_rho.bin").write_bytes(rho)

    print(
        f"[gen_data] ρ={RHO_BYTES}B y={y.shape} Â=({AHAT_POLYS},{N}) "
        f"Âsum={int(a_hat.sum())} ŷsum={int(y_hat.sum())}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
