#!/usr/bin/env python3
# coding=utf-8
"""
golden_c.py — Alg.14 全链 host golden：ek,m,coins → c.bin（1568B）。

流水线位置：stable-fips203-mlkem-pke-encrypt-k4 的 **黑盒 oracle**（FIPS 203 /
ML-KEM-1024 K-PKE.Encrypt）。仅提供合法期望输出，供 `run.sh` 对拍 `output/c.bin`；
**不是** AscendC 实现规格。

自包含 Python 参考，禁止 liboqs；与设备 G1–G4 语义对齐（SampleNTT / CBD / NTT /
内积 / INTT / μ / Compress+ByteEncode）。

I/O：
  输入 ek_pke[1568]、m[32]、coins[32]
  输出密文 c[1568] = ByteEncode(Compress(u)) ‖ ByteEncode(Compress(v))
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

XOF_BYTES = 672          # SampleNTT 单 poly：SHAKE128 挤出字节数
CAND_PAIRS = XOF_BYTES // 3  # 每 3B 解出 (d1,d2) 一对 12-bit 候选
PRF_BYTES = 128          # CBD η=2：每 poly 需要 128B PRF
PRF_BATCH = 9            # r(4)+e₁(4)+e₂(1)
EK_T_BYTES = 1536        # ek 前缀：ByteEncode₁₂(t̂)，尾 32B 为 ρ


def poly_byte_decode12(buf: bytes) -> np.ndarray:
    """ByteDecode₁₂：384B → 一 poly [256] int32（每 3B 解两系数）。"""
    out = np.empty(N, dtype=np.int32)
    for i in range(N // 2):
        b0, b1, b2 = buf[3 * i], buf[3 * i + 1], buf[3 * i + 2]
        t0 = b0 | ((b1 & 0x0F) << 8)
        t1 = (b1 >> 4) | (b2 << 4)
        out[2 * i] = t0
        out[2 * i + 1] = t1
    return out


def decode_t_hat(ek: bytes) -> np.ndarray:
    """从 ek_pke 前 1536B 解出 t̂[K,N]（Alg.14 行 2）。"""
    t = np.empty((K, N), dtype=np.int32)
    for j in range(K):
        t[j] = poly_byte_decode12(ek[j * 384 : (j + 1) * 384])
    return t


def shake128_squeeze(msg: bytes, outlen: int) -> bytes:
    """SHAKE128(msg) 挤出 outlen 字节（SampleNTT XOF）。"""
    return hashlib.shake_128(msg).digest(outlen)


def unpack_d12_from_xof(buf: bytes) -> tuple[np.ndarray, np.ndarray]:
    """XOF 字节流 → 两组 12-bit 候选 d1/d2（每 3B 一对）。"""
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
    """拒绝采样：依次接受 <q 的 d1/d2，凑满 N=256 系数；不足则失败。"""
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
    """Alg.14 行 3–7：ρ → SampleNTT → Â 平坦 [K*K*N]（行主序 p,j）。"""
    a_hat = np.empty(K * K * N, dtype=np.int32)
    for p in range(K):
        for j in range(K):
            # 种子：ρ ‖ j ‖ p（与 FIPS 203 / 设备 SampleNTT 一致）
            seed = rho + bytes([j & 0xFF, p & 0xFF])
            xof = shake128_squeeze(seed, XOF_BYTES)
            d1, d2 = unpack_d12_from_xof(xof)
            poly = rej_scalar_from_d12(d1, d2)
            off = (p * K + j) * N
            a_hat[off : off + N] = poly
    return a_hat


def prf_shake256(coins: bytes, nonce: int) -> bytes:
    """PRF：SHAKE256(coins‖nonce) → 128B（供 CBD η=2）。"""
    return hashlib.shake_256(coins + bytes([nonce & 0xFF])).digest(PRF_BYTES)


def _load32_le(buf: bytes, off: int) -> int:
    """小端 32-bit 装载（CBD 按字处理）。"""
    return int(buf[off]) | (int(buf[off + 1]) << 8) | (int(buf[off + 2]) << 16) | (int(buf[off + 3]) << 24)


def sample_poly_cbd2(buf: bytes) -> np.ndarray:
    """Alg.8 CBD η=2：128B → 一 poly [256]，系数映射到 [0,q)。"""
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
    """Alg.14 行 8–15：coins → r[K,N]、e₁[K,N]、e₂[N]（nonce 0..8）。"""
    rows = [sample_poly_cbd2(prf_shake256(coins, nonce)) for nonce in range(PRF_BATCH)]
    stacked = np.stack(rows)
    r = stacked[0:K]
    e1 = stacked[K : 2 * K]
    e2 = stacked[2 * K]
    return r, e1, e2


def golden_u_hat(a_hat: np.ndarray, r_hat: np.ndarray) -> np.ndarray:
    """NTT 域：û_p = Σ_j Â_{p,j} ⊙ r̂_j（逐对 MultiplyNTTs + mod q）。"""
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
    """NTT 域标量积：tr̂ = Σ_j t̂_j ⊙ r̂_j。"""
    acc = np.zeros(N, dtype=np.int64)
    for j in range(K):
        prod = multiply_ntts(t_hat[j], r_hat[j])
        acc += prod.astype(np.int64)
    return np.array([mod_q_i64(int(v)) for v in acc], dtype=np.int32)


def golden_encrypt(ek: bytes, m: bytes, coins: bytes) -> bytes:
    """
    Alg.14 全链：ek,m,coins → 密文 c（1568B）。
    步骤：解 ρ/t̂ → SampleNTT(Â) → CBD(r,e₁,e₂) → NTT(r) → û/tr̂ →
    INTT → +e₁ / (μ(m)+e₂) → Compress+ByteEncode。
    """
    rho = ek[1536:1568]
    t_hat = decode_t_hat(ek[:EK_T_BYTES])
    a_hat = build_a_hat(rho)
    r, e1, e2 = build_re(coins)
    r_hat = stage123_transform(r, "ntt")
    u_hat = golden_u_hat(a_hat, r_hat)
    tr_hat = golden_tr_hat(t_hat, r_hat)
    u = stage123_transform(u_hat, "intt")
    # tr̂ 单 poly：垫成 [K,N] 走同一 INTT 批，取第 0 行
    tr_pad = np.zeros((K, N), dtype=np.int32)
    tr_pad[0] = tr_hat
    tr = stage123_transform(tr_pad, "intt")[0]
    u = (u.astype(np.int64) + e1.astype(np.int64)) % Q
    u = u.astype(np.int32)
    v = embed_message(tr, m)  # v ← tr + μ(m)
    v = (v.astype(np.int64) + e2.astype(np.int64)) % Q
    v = v.astype(np.int32)
    return pack_ciphertext(u, v)


def main() -> None:
    """CLI：<ek_pke> <m.bin> <coins.bin> <golden_c.out>。"""
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
