#!/usr/bin/env python3
# coding=utf-8
"""本探针自包含 golden 原语（Alg.7 SampleNTT + Alg.14 PRF/CBD）。

代码自 stable / pass 探针抄写并固化于本目录；禁止 import 其它 ascendc-tests 用例或 library/shared。
仅依赖本目录 scripts/prep/alg7_geom.py（几何常量，与设备 f203_alg7_layout.h 同步）。
"""
from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

import numpy as np

_PREP = Path(__file__).resolve().parent / "prep"
if str(_PREP) not in sys.path:
    sys.path.insert(0, str(_PREP))

from alg7_geom import CAND_PAIRS, XOF_BYTES  # noqa: E402

KYBER_K = 4
KYBER_N = 256
KYBER_Q = 3329
ETA = 2
PRF_BYTES = (ETA * KYBER_N) // 4  # 128


# --- Alg.7 SampleNTT（抄自 stable scripts/prep/gen_data.py，路径已本地化）---


def shake128_squeeze(msg: bytes, outlen: int) -> bytes:

流水线位置：FIPS 203 Alg.14 / ML-KEM-1024 Encrypt；与 golden 对拍 c.bin。
"""SHAKE128 固定长度 squeeze（本探针 XOF_BYTES=672）。"""
    return hashlib.shake_128(msg).digest(outlen)


def unpack_d12_from_xof(buf: bytes) -> tuple[np.ndarray, np.ndarray]:
    """xof[672] → d1[224], d2[224]（Alg.7 步骤 6–7 全量 triple）。"""
    if len(buf) != XOF_BYTES:
        raise ValueError(f"expected {XOF_BYTES} bytes, got {len(buf)}")
    d1 = np.empty(CAND_PAIRS, dtype=np.int32)
    d2 = np.empty(CAND_PAIRS, dtype=np.int32)
    pos = 0
    for t in range(CAND_PAIRS):
        c0, c1, c2 = buf[pos], buf[pos + 1], buf[pos + 2]
        d1[t] = c0 + 256 * (c1 & 0x0F)
        d2[t] = (c1 >> 4) + 16 * c2
        pos += 3
    return d1, d2


def rej_scalar_from_d12(d1: np.ndarray, d2: np.ndarray, q: int = KYBER_Q, n: int = KYBER_N) -> np.ndarray:
    """规范顺序 rej：先 d1[t] 后 d2[t]，接受 v<q 直至 n 系数。"""
    out: list[int] = []
    for i in range(d1.shape[0]):
        v1 = int(d1[i])
        if v1 < q and len(out) < n:
            out.append(v1)
        v2 = int(d2[i])
        if v2 < q and len(out) < n:
            out.append(v2)
    if len(out) < n:
        raise SystemExit(f"rej scalar: only {len(out)} coeffs from {XOF_BYTES}B xof")
    return np.array(out[:n], dtype=np.int32)


def rej_bulk_from_d12(d1: np.ndarray, d2: np.ndarray, q: int = KYBER_Q, n: int = KYBER_N) -> np.ndarray:
    """批量 rej golden：全 stream 过滤后取前 n（与向量 rej 对拍）。"""
    stream: list[int] = []
    for i in range(d1.shape[0]):
        v1 = int(d1[i])
        stream.append(v1 if v1 < q else q)
        v2 = int(d2[i])
        stream.append(v2 if v2 < q else q)
    out = [x for x in stream if x < q]
    if len(out) < n:
        raise SystemExit(f"rej bulk: only {len(out)} coeffs")
    return np.array(out[:n], dtype=np.int32)


def sample_a_hat_poly(rho: bytes, p: int, j: int) -> np.ndarray:
    """单 poly SampleNTT：ρ||byte(j)||byte(p) → â[256]。"""
    seed = rho + bytes([j & 0xFF, p & 0xFF])
    xof = shake128_squeeze(seed, XOF_BYTES)
    d1, d2 = unpack_d12_from_xof(xof)
    a_spec = rej_scalar_from_d12(d1, d2)
    a_bulk = rej_bulk_from_d12(d1, d2)
    if not np.array_equal(a_spec, a_bulk):
        raise SystemExit(f"a_hat spec vs bulk mismatch at p={p} j={j}")
    return a_spec


def build_a_hat_from_rho(rho: bytes) -> np.ndarray:
    """16× SampleNTT → a_hat[16,256] 扁平 int32。"""
    a_hat = np.empty(KYBER_K * KYBER_K * KYBER_N, dtype=np.int32)
    for p in range(KYBER_K):
        for j in range(KYBER_K):
            poly = sample_a_hat_poly(rho, p, j)
            off = (p * KYBER_K + j) * KYBER_N
            a_hat[off : off + KYBER_N] = poly
    return a_hat


# --- Alg.14 PRF + CBD η=2（抄自 stable scripts/prep/fips203_se_sample/golden_se_sampling.py）---


def prf_shake256(key: bytes, nonce: int) -> bytes:
    """PRF(key, nonce) = SHAKE256(key || byte(nonce))，squeeze 128B。"""
    return hashlib.shake_256(key + bytes([nonce & 0xFF])).digest(PRF_BYTES)


def _load32_le(buf: bytes, off: int) -> int:
    return int(buf[off]) | (int(buf[off + 1]) << 8) | (int(buf[off + 2]) << 16) | (int(buf[off + 3]) << 24)


def sample_poly_cbd2(buf: bytes) -> np.ndarray:
    """SamplePolyCBD_η=2（FIPS 203 / Kyber 语义）。"""
    coeffs = np.zeros(KYBER_N, dtype=np.int32)
    for i in range(KYBER_N // 8):
        t = _load32_le(buf, 4 * i)
        d = (t & 0x55555555) + ((t >> 1) & 0x55555555)
        for j in range(8):
            a = (d >> (4 * j + 0)) & 0x3
            b = (d >> (4 * j + 2)) & 0x3
            c = a - b
            if c < 0:
                c += KYBER_Q
            coeffs[8 * i + j] = c % KYBER_Q
    return coeffs


def build_re_from_coins(coins: bytes, n_polys: int = 2 * KYBER_K + 1) -> np.ndarray:
    """coins → r(4)‖e₁(4)‖e₂(1) 共 9 poly，nonce 0..8。"""
    rows = []
    for nonce in range(n_polys):
        buf = prf_shake256(coins, nonce)
        if len(buf) != PRF_BYTES:
            raise SystemExit(f"PRF len {len(buf)} != {PRF_BYTES}")
        rows.append(sample_poly_cbd2(buf))
    return np.stack(rows)
