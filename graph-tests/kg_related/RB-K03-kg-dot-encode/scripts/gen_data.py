#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-K03 gen_data：自洽 SEED_D → Â/ŝ̂/ê̂/ρ + golden ek_pke/dk_pke（及 t̂）。

契约：
  - SEED_D=20260619 → Derand → G → ρ/σ；Â←SampleNTT；ŝ/ê←CBD
  - ŝ̂/ê̂ ← FIPS Alg.9 NTT（topology_math.mlkem_ntt）
  - t̂[p] = mod_q(Σ_j MultiplyNTTs(Â[p,j],ŝ̂[j]) + ê̂[p])
  - ek = BE₁₂(t̂)‖ρ；dk = BE₁₂(ŝ̂)
  - **不**把最终 ek/dk 写入 input（设备 MIX 才算）
  - γ：只读 ntt_onnx 表，落盘供设备 MultiplyNTTs
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
TOPO = _REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts"
# 后 insert 优先：ALG7 的 gen_data 须盖过 T10 同名 gen_data.py
sys.path.insert(0, str(TOPO))
sys.path.insert(0, str(SE_SHARED))
sys.path.insert(0, str(ALG7_SCRIPTS))

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
)
from topology_math import load_zetas_gammas, mlkem_ntt, mod_q, multiply_ntts  # noqa: E402

K = 4
N = 256
SEED_D_DEFAULT = 20260619
AHAT_POLYS = K * K
BE12_POLY = 384
EK_LEN = 1568
DK_LEN = 1536


def a_hat_offset(p: int, j: int) -> int:
    """行主序：Â[p,j] 扁平起始下标。"""
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


def byte_encode12(coeffs: np.ndarray) -> np.ndarray:
    """coeffs int32[256] → uint8[384]（FIPS Alg.5 d=12）。"""
    assert coeffs.shape == (N,)
    out = np.zeros(BE12_POLY, dtype=np.uint8)
    for i in range(N // 2):
        v0 = int(coeffs[2 * i]) & 0xFFF
        v1 = int(coeffs[2 * i + 1]) & 0xFFF
        out[3 * i + 0] = v0 & 0xFF
        out[3 * i + 1] = ((v0 >> 8) & 0x0F) | ((v1 & 0x0F) << 4)
        out[3 * i + 2] = (v1 >> 4) & 0xFF
    return out


def keygen_t_hat(a_hat: np.ndarray, s_ntt: np.ndarray, e_ntt: np.ndarray) -> np.ndarray:
    """
    t̂[p] = mod_q(Σ_j MultiplyNTTs(Â[p,j], ŝ̂[j]) + ê̂[p])。
    a_hat 扁平 [16*256]；s_ntt/e_ntt [4,256]。
    """
    assert a_hat.shape == (AHAT_POLYS * N,)
    assert s_ntt.shape == (K, N)
    assert e_ntt.shape == (K, N)
    out = np.zeros((K, N), dtype=np.int32)
    for p in range(K):
        acc = np.zeros(N, dtype=np.int64)
        for j in range(K):
            off = a_hat_offset(p, j)
            ap = a_hat[off : off + N]
            prod = multiply_ntts(ap.tolist(), s_ntt[j].tolist())
            acc += np.asarray(prod, dtype=np.int64)
        acc += e_ntt[p].astype(np.int64)
        out[p] = np.array([mod_q(int(x)) for x in acc], dtype=np.int32)
    return out


def main() -> int:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    os.environ["FIPS203_PRF_BACKEND"] = "shake256"

    d = derand_bytes_from_seed(seed_d, kyber_k=K)
    rho, sigma = hash_g_rho_sigma(d)
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
    s = src[:K].astype(np.int32)
    e = src[K:].astype(np.int32)

    s_ntt = np.stack([np.asarray(mlkem_ntt(s[i].tolist()), dtype=np.int32) for i in range(K)], axis=0)
    e_ntt = np.stack([np.asarray(mlkem_ntt(e[i].tolist()), dtype=np.int32) for i in range(K)], axis=0)

    _zetas, gammas_list = load_zetas_gammas()
    gammas = np.asarray(gammas_list, dtype=np.int32)

    t_hat = keygen_t_hat(a_hat, s_ntt, e_ntt)

    ek = np.zeros(EK_LEN, dtype=np.uint8)
    dk = np.zeros(DK_LEN, dtype=np.uint8)
    for p in range(K):
        ek[p * BE12_POLY : (p + 1) * BE12_POLY] = byte_encode12(t_hat[p])
        dk[p * BE12_POLY : (p + 1) * BE12_POLY] = byte_encode12(s_ntt[p])
    ek[1536:1568] = np.frombuffer(rho, dtype=np.uint8)

    rng = np.random.default_rng(20260909)
    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)

    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    (inp / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    a_hat.tofile(inp / "a_hat.bin")
    s_ntt.tofile(inp / "s_ntt.bin")
    e_ntt.tofile(inp / "e_ntt.bin")
    gammas.tofile(inp / "gammas.bin")
    (inp / "rho.bin").write_bytes(rho)
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    t_hat.tofile(out / "golden_t_hat.bin")
    ek.tofile(out / "golden_ek_pke.bin")
    dk.tofile(out / "golden_dk_pke.bin")

    print(
        f"[gen_data] SEED_D={seed_d} a_hat={a_hat.shape} "
        f"t_hat sum={int(t_hat.sum())} ek={EK_LEN} dk={DK_LEN}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
