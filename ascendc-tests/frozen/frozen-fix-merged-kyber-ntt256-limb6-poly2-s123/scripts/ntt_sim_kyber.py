#!/usr/bin/env python3
# coding=utf-8
"""merged_kyber 6bit limb NTT 参考（Split + 2×MMAD + Barrett Merge 与 limb6 设备同构）。"""
from __future__ import annotations

import numpy as np

LIMB_BITS = 6
LIMB_MASK = (1 << LIMB_BITS) - 1
Q = 3329
N = 256
BARRETT_K = 12
BARRETT_MU = 5039


def _log2n(n: int) -> int:
    b = 0
    t = n
    while t > 1:
        b += 1
        t >>= 1
    return b


def _rev_bits(i: int, bits: int) -> int:
    r = 0
    for _ in range(bits):
        r = (r << 1) | (i & 1)
        i >>= 1
    return r


def _pow_mod(base: int, exp: int, mod: int) -> int:
    r = 1
    base %= mod
    while exp:
        if exp & 1:
            r = (r * base) % mod
        base = (base * base) % mod
        exp >>= 1
    return r


def _primitive_root(mod: int) -> int:
    phi = mod - 1
    factors = []
    x = phi
    d = 2
    while d * d <= x:
        if x % d == 0:
            factors.append(d)
            while x % d == 0:
                x //= d
        d += 1 if d == 2 else 2
    if x > 1:
        factors.append(x)
    for g in range(2, mod):
        if all(_pow_mod(g, phi // p, mod) != 1 for p in factors):
            return g
    raise RuntimeError(f"no primitive root for q={mod}")


def get_2n_th_root(q: int, n: int) -> int:
    g = _primitive_root(q)
    return _pow_mod(g, (q - 1) // (2 * n), q)


def build_vandermonde(n: int, q: int) -> np.ndarray:
    psi = get_2n_th_root(q, n)
    bits = _log2n(n)
    psi_pow = np.zeros(2 * n, dtype=np.int64)
    psi_pow[0] = 1
    for i in range(1, 2 * n):
        psi_pow[i] = (psi_pow[i - 1] * psi) % q
    m = np.zeros((n, n), dtype=np.int32)
    for k in range(n):
        br_k = _rev_bits(k, bits)
        row_base = 2 * br_k + 1
        for j in range(n):
            exp = (j * row_base) % (2 * n)
            m[k, j] = int(psi_pow[exp])
    return m


def _wrap_mod_vec(dst: np.ndarray, q: int) -> None:
    t1 = dst - q
    t2 = (t1 >> 63) & dst
    np.maximum(t1, t2, out=dst)


def _barrett_mul_vec(dst: np.ndarray, q: int, k: int, mu: int) -> None:
    t1 = dst >> (k - 1)
    t1 = t1 * mu
    t1 = t1 >> (k + 1)
    t1 = t1 * q
    dst -= t1
    _wrap_mod_vec(dst, q)


def _barrett_mod_q(acc: np.ndarray, q: int = Q) -> np.ndarray:
    out = acc.astype(np.int64).copy()
    _barrett_mul_vec(out, q, BARRETT_K, BARRETT_MU)
    _barrett_mul_vec(out, q, BARRETT_K, BARRETT_MU)
    return (out % q).astype(np.int32)


def split_s0(f: np.ndarray) -> np.ndarray:
    """[N] int32 → S0 [2,N] int8；row0=lo，row1=hi（与 split_vec / AivSplit CopyOut 一致）。"""
    v = f.astype(np.int64) % Q
    s0 = np.zeros((2, N), dtype=np.int8)
    s0[0] = (v & LIMB_MASK).astype(np.int8)
    s0[1] = ((v >> LIMB_BITS) & LIMB_MASK).astype(np.int8)
    return s0


def mmad_rows(s0: np.ndarray, m_plane: np.ndarray) -> np.ndarray:
    """S0 [m,N] int8 × M_plane [N,N] int8 → [m,N] int32。"""
    return (s0.astype(np.int32) @ m_plane.astype(np.int32)).astype(np.int32)


def merge_rows(a0: np.ndarray, a1: np.ndarray, q: int = Q) -> np.ndarray:
    """A0/A1 各 [2,N] int32 → NTT [N] int32（行 2/3 视为 0）。"""
    acc = (
        a0[0].astype(np.int64)
        + (a0[1].astype(np.int64) << LIMB_BITS)
        + (a1[0].astype(np.int64) << LIMB_BITS)
        + (a1[1].astype(np.int64) << (2 * LIMB_BITS))
    )
    return _barrett_mod_q(acc, q)


def ntt_test01(n: int, q: int, g: int, f: np.ndarray) -> np.ndarray:
    """设备同构三段式 golden（g 保留接口；M 由 NWC 范德蒙生成）。"""
    del g
    m = build_vandermonde(n, q)
    m0 = ((m >> 0) & LIMB_MASK).astype(np.int8)
    m1 = ((m >> LIMB_BITS) & LIMB_MASK).astype(np.int8)
    s0 = split_s0(f)
    a0 = mmad_rows(s0, m0)
    a1 = mmad_rows(s0, m1)
    return merge_rows(a0, a1, q)


def ntt_forward(f: np.ndarray, n: int, q: int, g: int) -> np.ndarray:
    """merged_kyber golden：f @ M mod q（与 AicMmad S0×M 列方向一致，非 ntt_study MatMulInt32 的 M·a）。"""
    del g
    m = build_vandermonde(n, q)
    v = f.astype(np.int64) % q
    out = (v @ m.astype(np.int64)) % q
    return out.astype(np.int32)


M = build_vandermonde(N, Q)
