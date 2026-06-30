#!/usr/bin/env python3
"""
gen_ek_pke.py — vendored KeyGen golden 生成 ek_pke（1568B，自包含，禁止 liboqs）。

逻辑抄写 pass-fix-f203-alg13-device-keygen-k4/scripts/keygen_golden.py 语义，
仅保留 ek 生产所需：ρ→Â、σ→s/e→NTT→t̂→ByteEncode₁₂‖ρ。
"""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

import numpy as np

from f203_ref_common import (
    K,
    N,
    Q,
    XOF_BYTES,
    CAND_PAIRS,
    a_hat_offset,
    multiply_ntts,
    mod_q_i64,
    poly_byte_encode12,
    stage123_transform,
)

EK_BYTES = 1568
POLY_D12_BYTES = 384


def derand_bytes_from_seed(seed_d: int) -> bytes:
    msg = f"exp-mlkem-f203-2s1e-k4:SEED_D={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def hash_g_rho_sigma(d: bytes) -> tuple[bytes, bytes]:
    buf = hashlib.sha3_512(d + bytes([K & 0xFF])).digest()
    return buf[:32], buf[32:64]


def shake128_squeeze(msg: bytes, outlen: int) -> bytes:
    return hashlib.shake_128(msg).digest(outlen)


def unpack_d12_from_xof(buf: bytes) -> tuple[np.ndarray, np.ndarray]:
    d1 = np.empty(CAND_PAIRS, dtype=np.int32)
    d2 = np.empty(CAND_PAIRS, dtype=np.int32)
    pos = 0
    for t in range(CAND_PAIRS):
        c0, c1, c2 = buf[pos], buf[pos + 1], buf[pos + 2]
        d1[t] = c0 + 256 * (c1 & 0x0F)
        d2[t] = (c1 >> 4) + 16 * c2
        pos += 3
    return d1, d2


def rej_scalar_from_d12(d1: np.ndarray, d2: np.ndarray) -> np.ndarray:
    out: list[int] = []
    for i in range(d1.shape[0]):
        v1 = int(d1[i])
        if v1 < Q and len(out) < N:
            out.append(v1)
        v2 = int(d2[i])
        if v2 < Q and len(out) < N:
            out.append(v2)
    if len(out) < N:
        raise SystemExit(f"rej: only {len(out)} coeffs")
    return np.array(out[:N], dtype=np.int32)


def build_a_hat(rho: bytes) -> np.ndarray:
    polys = K * K
    a_hat = np.empty(polys * N, dtype=np.int32)
    for p in range(K):
        for j in range(K):
            seed = rho + bytes([j & 0xFF, p & 0xFF])
            xof = shake128_squeeze(seed, XOF_BYTES)
            d1, d2 = unpack_d12_from_xof(xof)
            poly = rej_scalar_from_d12(d1, d2)
            off = a_hat_offset(p, j)
            a_hat[off : off + N] = poly
    return a_hat


def prf_shake256(sigma: bytes, nonce: int) -> bytes:
    return hashlib.shake_256(sigma + bytes([nonce & 0xFF])).digest(128)


def _load32_le(buf: bytes, off: int) -> int:
    return int(buf[off]) | (int(buf[off + 1]) << 8) | (int(buf[off + 2]) << 16) | (int(buf[off + 3]) << 24)


def sample_poly_cbd2(buf: bytes) -> np.ndarray:
    coeffs = np.zeros(N, dtype=np.int32)
    for i in range(N // 8):
        t = _load32_le(buf, 4 * i)
        d = (t & 0x55555555) + ((t >> 1) & 0x55555555)
        for j in range(8):
            a = (d >> (4 * j + 0)) & 0x3
            b = (d >> (4 * j + 2)) & 0x3
            c = a - b
            if c < 0:
                c += Q
            coeffs[8 * i + j] = c % Q
    return coeffs


def build_src(sigma: bytes) -> np.ndarray:
    rows = []
    nonce = 0
    for _ in range(K):
        rows.append(sample_poly_cbd2(prf_shake256(sigma, nonce)))
        nonce += 1
    for _ in range(K):
        rows.append(sample_poly_cbd2(prf_shake256(sigma, nonce)))
        nonce += 1
    return np.stack(rows)


def golden_t_hat(a_hat_flat: np.ndarray, s_hat: np.ndarray, e_hat: np.ndarray) -> np.ndarray:
    t = np.zeros((K, N), dtype=np.int32)
    for p in range(K):
        acc = np.zeros(N, dtype=np.int64)
        for j in range(K):
            off = a_hat_offset(p, j)
            a_poly = a_hat_flat[off : off + N]
            prod = multiply_ntts(a_poly, s_hat[j])
            acc += prod.astype(np.int64)
        acc += e_hat[p].astype(np.int64)
        t[p] = np.array([mod_q_i64(int(v)) for v in acc], dtype=np.int32)
    return t


def build_ek_pke(seed_d: int) -> np.ndarray:
    d = derand_bytes_from_seed(seed_d)
    rho, sigma = hash_g_rho_sigma(d)
    a_hat = build_a_hat(rho)
    src = build_src(sigma)
    s = src[:K]
    e = src[K:]
    s_hat = stage123_transform(s, "ntt")
    e_hat = stage123_transform(e, "ntt")
    t_hat = golden_t_hat(a_hat, s_hat, e_hat)
    ek_polyvec = bytearray(K * POLY_D12_BYTES)
    for j in range(K):
        ek_polyvec[j * POLY_D12_BYTES : (j + 1) * POLY_D12_BYTES] = poly_byte_encode12(t_hat[j])
    ek = np.frombuffer(bytes(ek_polyvec) + rho, dtype=np.uint8)
    if ek.size != EK_BYTES:
        raise SystemExit(f"ek size {ek.size} != {EK_BYTES}")
    return ek


def main() -> None:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <SEED_D> <ek_pke.out>", file=sys.stderr)
        sys.exit(1)
    seed_d = int(sys.argv[1])
    ek_path = Path(sys.argv[2])
    ek = build_ek_pke(seed_d)
    ek.tofile(ek_path)
    print(f"[gen_ek_pke] KeyGen golden ek {EK_BYTES}B SEED_D={seed_d}")


if __name__ == "__main__":
    main()
