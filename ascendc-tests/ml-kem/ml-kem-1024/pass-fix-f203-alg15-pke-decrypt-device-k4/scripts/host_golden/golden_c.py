#!/usr/bin/env python3
"""
golden_c.py — FIPS 203 Alg.14 全链 Host golden：ek + m + coins → c（1568B）。

流水线位置：gen_data 用夹具 ek/m/coins 造 input/c.bin（Decrypt 生产输入）。
语义：Decode(t̂,ρ)→Â；CBD(r,e1,e2)；NTT→û/tr̂→INTT→加噪→Compress/Encode→c。
自包含 Python，禁止 liboqs；仅造合法密文，非 AscendC 规格。
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
    a_hat_offset_at,
    embed_message,
    mod_q_i64,
    multiply_ntts,
    pack_ciphertext,
    stage123_transform,
)

XOF_BYTES = 672
CAND_PAIRS = XOF_BYTES // 3
PRF_BYTES = 128
PRF_BATCH = 9
EK_T_BYTES = 1536


def poly_byte_decode12(buf: bytes) -> np.ndarray:
    """ByteDecode₁₂：384B → n 个 12-bit 系数。"""
    out = np.empty(N, dtype=np.int32)
    for i in range(N // 2):
        b0, b1, b2 = buf[3 * i], buf[3 * i + 1], buf[3 * i + 2]
        t0 = b0 | ((b1 & 0x0F) << 8)
        t1 = (b1 >> 4) | (b2 << 4)
        out[2 * i] = t0
        out[2 * i + 1] = t1
    return out


def decode_t_hat(ek: bytes) -> np.ndarray:
    """ek 前 1536B → t̂[k,n]（不含尾部 ρ）。"""
    t = np.empty((K, N), dtype=np.int32)
    for j in range(K):
        t[j] = poly_byte_decode12(ek[j * 384 : (j + 1) * 384])
    return t


def shake128_squeeze(msg: bytes, outlen: int) -> bytes:
    """SHAKE128 XOF 挤出。"""
    return hashlib.shake_128(msg).digest(outlen)


def unpack_d12_from_xof(buf: bytes) -> tuple[np.ndarray, np.ndarray]:
    """XOF → 12-bit 候选对。"""
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
    """拒绝采样凑满 n 系数。"""
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
    """ρ → Â 扁平 k*k*n（行主：p,j）。"""
    a_hat = np.empty(K * K * N, dtype=np.int32)
    for p in range(K):
        for j in range(K):
            seed = rho + bytes([j & 0xFF, p & 0xFF])
            xof = shake128_squeeze(seed, XOF_BYTES)
            d1, d2 = unpack_d12_from_xof(xof)
            poly = rej_scalar_from_d12(d1, d2)
            off = (p * K + j) * N
            a_hat[off : off + N] = poly
    return a_hat


def prf_shake256(coins: bytes, nonce: int) -> bytes:
    """Encrypt PRF：SHAKE256(coins‖nonce) → 128B。"""
    return hashlib.shake_256(coins + bytes([nonce & 0xFF])).digest(PRF_BYTES)


def _load32_le(buf: bytes, off: int) -> int:
    """小端 uint32。"""
    return int(buf[off]) | (int(buf[off + 1]) << 8) | (int(buf[off + 2]) << 16) | (int(buf[off + 3]) << 24)


def sample_poly_cbd2(buf: bytes) -> np.ndarray:
    """η=2 CBD → n 系数。"""
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


def build_re(coins: bytes) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """coins → (r[k], e1[k], e2) 共 9 个 CBD2 poly。"""
    rows = [sample_poly_cbd2(prf_shake256(coins, nonce)) for nonce in range(PRF_BATCH)]
    stacked = np.stack(rows)
    r = stacked[0:K]
    e1 = stacked[K : 2 * K]
    e2 = stacked[2 * K]
    return r, e1, e2


def golden_u_hat(a_hat: np.ndarray, r_hat: np.ndarray) -> np.ndarray:
    """û_p = Σ_j MultiplyNTTs(Â_{j,p 布局}, r̂_j)（Encrypt 矩阵下标）。"""
    u = np.zeros((K, N), dtype=np.int32)
    for p in range(K):
        acc = np.zeros(N, dtype=np.int64)
        for j in range(K):
            off = a_hat_offset_at(p, j)
            prod = multiply_ntts(a_hat[off : off + N], r_hat[j])
            acc += prod.astype(np.int64)
        u[p] = np.array([mod_q_i64(int(v)) for v in acc], dtype=np.int32)
    return u


def golden_tr_hat(t_hat: np.ndarray, r_hat: np.ndarray) -> np.ndarray:
    """⟨t̂, r̂⟩ NTT 域内积 → 单 poly。"""
    acc = np.zeros(N, dtype=np.int64)
    for j in range(K):
        prod = multiply_ntts(t_hat[j], r_hat[j])
        acc += prod.astype(np.int64)
    return np.array([mod_q_i64(int(v)) for v in acc], dtype=np.int32)


def golden_encrypt(ek: bytes, m: bytes, coins: bytes) -> bytes:
    """
    Alg.14 全链 → c = c₁‖c₂（1568B）。

    步骤：解 t̂/ρ → Â → 采样 r,e → NTT → û/tr̂ → INTT → +e → embed m → pack。
    """
    rho = ek[1536:1568]
    t_hat = decode_t_hat(ek[:EK_T_BYTES])
    a_hat = build_a_hat(rho)
    r, e1, e2 = build_re(coins)
    r_hat = stage123_transform(r, "ntt")
    u_hat = golden_u_hat(a_hat, r_hat)
    tr_hat = golden_tr_hat(t_hat, r_hat)
    u = stage123_transform(u_hat, "intt")
    tr_pad = np.zeros((K, N), dtype=np.int32)
    tr_pad[0] = tr_hat
    tr = stage123_transform(tr_pad, "intt")[0]
    u = (u.astype(np.int64) + e1.astype(np.int64)) % Q
    u = u.astype(np.int32)
    v = embed_message(tr, m)
    v = (v.astype(np.int64) + e2.astype(np.int64)) % Q
    v = v.astype(np.int32)
    return pack_ciphertext(u, v)


def main() -> None:
    """CLI：ek + m + coins → golden_c.out。"""
    if len(sys.argv) != 5:
        print(f"usage: {sys.argv[0]} <ek_pke> <m.bin> <coins.bin> <golden_c.out>", file=sys.stderr)
        sys.exit(1)
    ek = Path(sys.argv[1]).read_bytes()
    m = Path(sys.argv[2]).read_bytes()
    coins = Path(sys.argv[3]).read_bytes()
    out_path = Path(sys.argv[4])
    if len(ek) != 1568 or len(m) != 32 or len(coins) != 32:
        raise SystemExit(f"bad input sizes ek={len(ek)} m={len(m)} coins={len(coins)}")
    c = golden_encrypt(ek, m, coins)
    if len(c) != 1568:
        raise SystemExit(f"golden c size {len(c)} != 1568")
    out_path.write_bytes(c)
    print(f"[golden_c] OK {len(c)}B")


if __name__ == "__main__":
    main()
