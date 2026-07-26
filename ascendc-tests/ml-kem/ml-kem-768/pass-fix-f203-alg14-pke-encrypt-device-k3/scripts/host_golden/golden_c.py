#!/usr/bin/env python3
"""
@file golden_c.py
@brief Alg.14 全链 host golden：ek,m,coins → c.bin（1088B）。

流水线：device 探针 scripts/gen_data.py 调用；自包含 Python 参考，禁止 liboqs。
与设备 G1–G4 语义对齐；验收仅 I/O 等价，非实现同构。

Alg.14 步骤对应：
  行 2  decode t̂；行 3–15 SampleNTT(Â)+CBD(r,e₁,e₂)；
  行 16–19 NTT/内积/INTT+e₁；行 20–21 μ 嵌入与 +e₂；行 22–24 pack → c。
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
PRF_BATCH = 7  # r(3)+e₁(3)+e₂(1)
EK_T_BYTES = 1152  # ByteEncode₁₂(t̂) 不含 ρ


def poly_byte_decode12(buf: bytes) -> np.ndarray:
    """
    Alg.6 ByteDecode₁₂：384B → int32[N]。
    每 3 字节解出两个 12-bit 系数（小端交错）。
    """
    out = np.empty(N, dtype=np.int32)
    for i in range(N // 2):
        b0, b1, b2 = buf[3 * i], buf[3 * i + 1], buf[3 * i + 2]
        t0 = b0 | ((b1 & 0x0F) << 8)
        t1 = (b1 >> 4) | (b2 << 4)
        out[2 * i] = t0
        out[2 * i + 1] = t1
    return out


def decode_t_hat(ek: bytes) -> np.ndarray:
    """
    行 2：从 ek_pke 前 1152B 解出 t̂[K,N]。
    @param ek  完整 ek_pke（1184B）；本函数只用前 EK_T_BYTES
    """
    t = np.empty((K, N), dtype=np.int32)
    for j in range(K):
        t[j] = poly_byte_decode12(ek[j * 384 : (j + 1) * 384])
    return t


def shake128_squeeze(msg: bytes, outlen: int) -> bytes:
    """SHAKE128 XOF：SampleNTT 用。"""
    return hashlib.shake_128(msg).digest(outlen)


def unpack_d12_from_xof(buf: bytes) -> tuple[np.ndarray, np.ndarray]:
    """
    将 XOF 字节流拆成候选对 (d1,d2)，每 3 字节一对 12-bit。
    @return d1,d2 各长度 CAND_PAIRS
    """
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
    """
    Alg.7 拒绝采样：依次取 d1/d2 中 <q 的值，凑满 N 个系数。
    不足 N 则失败退出（golden 路径 XOF 长度已按最坏情况取足）。
    """
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
    """
    行 3–6：ρ → Â[K×K×N]（SampleNTT）。
    布局：off=(p*K+j)*N（与 gen_ek 的 a_hat_offset 一致；设备 handoff 用 a_hat_offset_at）。
    """
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
    """Alg.8 前：PRF(coins‖nonce) → 128B CBD 输入。"""
    return hashlib.shake_256(coins + bytes([nonce & 0xFF])).digest(PRF_BYTES)


def _load32_le(buf: bytes, off: int) -> int:
    """小端读 32-bit 字（CBD 比特抽取用）。"""
    return int(buf[off]) | (int(buf[off + 1]) << 8) | (int(buf[off + 2]) << 16) | (int(buf[off + 3]) << 24)


def sample_poly_cbd2(buf: bytes) -> np.ndarray:
    """
    Alg.8 η=2 CBD：128B → int32[N]（中心二项，再 mod q）。
    每 4 字节产 8 个系数。
    """
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
    """
    行 7–15：coins → r[K,N], e₁[K,N], e₂[N]（nonce 0..6）。
    """
    rows = [sample_poly_cbd2(prf_shake256(coins, nonce)) for nonce in range(PRF_BATCH)]
    stacked = np.stack(rows)
    r = stacked[0:K]
    e1 = stacked[K : 2 * K]
    e2 = stacked[2 * K]
    return r, e1, e2


def golden_u_hat(a_hat: np.ndarray, r_hat: np.ndarray) -> np.ndarray:
    """
    行 18：û ← Âᵀ ∘ r̂（NTT 域内积累加）。
    使用 a_hat_offset_at(p,j)=(j*K+p)*N，与设备 prep 存储 / handoff 一致。
    """
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
    """行 19 前半：tr̂ ← ⟨t̂, r̂⟩（NTT 域点积）。"""
    acc = np.zeros(N, dtype=np.int64)
    for j in range(K):
        prod = multiply_ntts(t_hat[j], r_hat[j])
        acc += prod.astype(np.int64)
    return np.array([mod_q_i64(int(v)) for v in acc], dtype=np.int32)


def golden_encrypt(ek: bytes, m: bytes, coins: bytes) -> bytes:
    """
    Alg.14 全链参考：ek+m+coins → c（1088B）。
    分段：decode → SampleNTT/CBD → NTT(r) → û/tr̂ → INTT+噪声 → μ 嵌入 → pack。
    """
    # --- 行 2–3：拆 ρ / t̂ ---
    rho = ek[1152:1184]
    t_hat = decode_t_hat(ek[:EK_T_BYTES])
    # --- 行 3–15：Â 与 r‖e₁‖e₂ ---
    a_hat = build_a_hat(rho)
    r, e1, e2 = build_re(coins)
    # --- 行 16–18：NTT(r) 与 û ---
    r_hat = stage123_transform(r, "ntt")
    u_hat = golden_u_hat(a_hat, r_hat)
    tr_hat = golden_tr_hat(t_hat, r_hat)
    # --- 行 19：INTT + e₁；tr 单 poly 经 pad 走同一 Stage123 ---
    u = stage123_transform(u_hat, "intt")
    tr_pad = np.zeros((K, N), dtype=np.int32)
    tr_pad[0] = tr_hat
    tr = stage123_transform(tr_pad, "intt")[0]
    u = (u.astype(np.int64) + e1.astype(np.int64)) % Q
    u = u.astype(np.int32)
    # --- 行 20–21：v ← Decompress₁(m) + tr + e₂ ---
    v = embed_message(tr, m)
    v = (v.astype(np.int64) + e2.astype(np.int64)) % Q
    v = v.astype(np.int32)
    # --- 行 22–24：Compress/ByteEncode → c ---
    return pack_ciphertext(u, v)


def main() -> None:
    """CLI：golden_c.py <ek_pke> <m.bin> <coins.bin> <golden_c.out>"""
    if len(sys.argv) != 5:
        print(f"usage: {sys.argv[0]} <ek_pke> <m.bin> <coins.bin> <golden_c.out>", file=sys.stderr)
        sys.exit(1)
    ek = Path(sys.argv[1]).read_bytes()
    m = Path(sys.argv[2]).read_bytes()
    coins = Path(sys.argv[3]).read_bytes()
    out_path = Path(sys.argv[4])
    if len(ek) != 1184 or len(m) != 32 or len(coins) != 32:
        raise SystemExit(f"bad input sizes ek={len(ek)} m={len(m)} coins={len(coins)}")
    c = golden_encrypt(ek, m, coins)
    if len(c) != 1088:
        raise SystemExit(f"golden c size {len(c)} != 1088")
    out_path.write_bytes(c)
    print(f"[golden_c] OK {len(c)}B")


if __name__ == "__main__":
    main()
