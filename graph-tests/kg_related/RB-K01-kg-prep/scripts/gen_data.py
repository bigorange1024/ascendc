#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_data — RB-K01 KeyGen prep oracle（Host / FIPS 契约，本刀不接 liboqs 全链）。

契约（与仓内 SEED_D / Derand 一致）：
  SEED_D → d = SHA3-256("exp-mlkem-f203-2s1e-k4:SEED_D=<dec>")
        → (ρ‖σ) = SHA3-512(d‖byte(k=4))
  Â[16,256] ← SampleNTT(ρ‖j‖i)，XOF=SHAKE128 672B（活跃 alg7_geom）
  src[8,256] ← CBD_η2(PRF_SHAKE256(σ,N))；行 0..3=ŝ，4..7=ê
"""
from __future__ import annotations

import os
import struct
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

ALG7_SCRIPTS = (
    _REPO
    / "ascendc-tests"
    / "ml-kem"
    / "ml-kem-1024"
    / "pass-fix-f203-alg7-sample-ntt-k4"
    / "scripts"
)
SE_SHARED = _REPO / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(ALG7_SCRIPTS))
sys.path.insert(0, str(SE_SHARED))

from alg7_geom import XOF_BYTES  # noqa: E402
from gen_data import (  # noqa: E402
    rej_bulk_from_d12,
    rej_scalar_from_d12,
    shake128_squeeze,
    unpack_d12_from_xof,
)
from golden_se_sampling import (  # noqa: E402
    build_src,
    derand_bytes_from_seed,
    hash_g_sigma,
    prf_shake256,
)

K = 4
N = 256
SEED_D_DEFAULT = 20260619
AHAT_POLYS = K * K
PRF_ROWS = 8
PRF_BYTES = 128


def a_hat_offset(p: int, j: int) -> int:
    """行主序：Â[p,j] 扁平起始下标（与 lines3-7 / vec-k4 契约一致）。"""
    return (p * K + j) * N


def hash_g_rho_sigma(d: bytes) -> tuple[bytes, bytes]:
    """G(d‖k) → (ρ,σ)。"""
    import hashlib

    buf = hashlib.sha3_512(d + bytes([K & 0xFF])).digest()
    return buf[:32], buf[32:64]


def sample_one_poly(rho: bytes, p: int, j: int) -> np.ndarray:
    """Alg.7 SampleNTT：seed=ρ‖byte(j)‖byte(i=p)。"""
    seed = rho + bytes([j & 0xFF, p & 0xFF])
    xof = shake128_squeeze(seed, XOF_BYTES)
    d1, d2 = unpack_d12_from_xof(xof)
    a_spec = rej_scalar_from_d12(d1, d2)
    a_bulk = rej_bulk_from_d12(d1, d2)
    if not np.array_equal(a_spec, a_bulk):
        raise SystemExit(f"spec vs bulk mismatch at p={p} j={j}")
    return a_spec


def build_prf_out(sigma: bytes) -> np.ndarray:
    rows = [np.frombuffer(prf_shake256(sigma, nonce), dtype=np.uint8) for nonce in range(PRF_ROWS)]
    return np.stack(rows)


def main() -> int:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    # 与 lines8-15 / 设备 PRF 积木一致：SHAKE256
    os.environ["FIPS203_PRF_BACKEND"] = "shake256"

    d = derand_bytes_from_seed(seed_d, kyber_k=K)
    rho, sigma = hash_g_rho_sigma(d)
    # 交叉：hash_g_sigma 应等于 σ 半段
    if hash_g_sigma(d) != sigma:
        raise SystemExit("rho/sigma split mismatch vs golden_se_sampling.hash_g_sigma")

    a_hat = np.empty(AHAT_POLYS * N, dtype=np.int32)
    for p in range(K):
        for j in range(K):
            poly = sample_one_poly(rho, p, j)
            off = a_hat_offset(p, j)
            a_hat[off : off + N] = poly

    src = build_src(seed_d).astype(np.int32)
    if src.shape != (8, N):
        raise SystemExit(f"unexpected src shape {src.shape}")
    s_hat = src[:K].reshape(-1)
    e = src[K:].reshape(-1)
    prf_out = build_prf_out(sigma)

    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    (inp / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    a_hat.tofile(out / "golden_a_hat.bin")
    s_hat.tofile(out / "golden_s_hat.bin")
    e.tofile(out / "golden_e.bin")
    src.reshape(-1).tofile(out / "golden_src.bin")
    prf_out.tofile(out / "golden_prf_out.bin")
    (out / "golden_rho.bin").write_bytes(rho)
    (out / "golden_sigma.bin").write_bytes(sigma)

    print(
        f"[gen_data] SEED_D={seed_d} XOF={XOF_BYTES} "
        f"a_hat=({AHAT_POLYS},{N}) src=(8,{N}) PRF=shake256"
    )
    print(f"[gen_data] a_hat sum={int(a_hat.sum())} s_hat sum={int(s_hat.sum())} e sum={int(e.sum())}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
