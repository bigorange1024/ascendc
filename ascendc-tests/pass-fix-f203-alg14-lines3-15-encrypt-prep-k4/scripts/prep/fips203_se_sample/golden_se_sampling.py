#!/usr/bin/env python3
# @probe stable-fips203-mlkem-pke-keygen-k4
# @file scripts/prep/fips203_se_sample/golden_se_sampling.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `golden_se_sampling.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify run.sh 编译前生成 input/；KEYGEN_VERIFY=1 时参与对拍。

# coding=utf-8
"""Python golden for Alg.13 行 8–15 — 与 fips203_prf / fips203_se_sample.c 同语义。

流水线位置（KeyGen/presample 对照）：SEED_D → derand → σ → PRF×8 → CBD_η=2 → src[8,256]。
Encrypt prep 主 golden 在 scripts/golden_encrypt_prep.py（coins→9 poly，含 e₂）；
本模块保留作 vendoring 对照与 scripts/prep/gen_data 等历史入口。

与设备：黑盒 I/O 等价即可；禁止当作 AscendC 逐步规格。
"""
from __future__ import annotations

import hashlib
import os

import numpy as np

Q = 3329
N = 256
K = 4
ETA = 2
SEED_D_DEFAULT = 20260619


def derand_bytes_from_seed(seed_d: int) -> bytes:
    """域分离 SHA3-256：整数 SEED_D → 32B derand 种子。"""
    msg = f"exp-mlkem-f203-2s1e-k4:SEED_D={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def hash_g_sigma(d: bytes) -> bytes:
    """G(d)=SHA3-512(d‖byte(k)) 后 32B → σ。"""
    buf = hashlib.sha3_512(d + bytes([K & 0xFF])).digest()
    return buf[32:64]


def prf_shake128(sigma: bytes, nonce: int) -> bytes:
    """历史对照：SHAKE128 PRF（Encrypt/FIPS 203 生产用 SHAKE256）。"""
    outlen = ETA * N // 4
    return hashlib.shake_128(sigma + bytes([nonce & 0xFF])).digest(outlen)


def prf_shake256(sigma: bytes, nonce: int) -> bytes:
    """PRF = SHAKE256(σ‖nonce)，squeeze η·N/4 字节。"""
    outlen = ETA * N // 4
    return hashlib.shake_256(sigma + bytes([nonce & 0xFF])).digest(outlen)


def _prf(sigma: bytes, nonce: int) -> bytes:
    """按 FIPS203_PRF_BACKEND 选择 shake128/shake256（默认 shake128 历史对照）。"""
    backend = os.environ.get("FIPS203_PRF_BACKEND", "shake128").lower()
    if backend == "shake256":
        return prf_shake256(sigma, nonce)
    return prf_shake128(sigma, nonce)


def _load32_le(buf: bytes, off: int) -> int:
    """小端 load32。"""
    return int(buf[off]) | (int(buf[off + 1]) << 8) | (int(buf[off + 2]) << 16) | (int(buf[off + 3]) << 24)


def sample_poly_cbd2(buf: bytes) -> np.ndarray:
    """SamplePolyCBD_η=2 → [256] int32。"""
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


def build_src(seed_d: int | None = None) -> np.ndarray:
    """SEED_D → s(4)‖e(4) 共 8 poly（nonce 0..7）。"""
    if seed_d is None:
        seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    d = derand_bytes_from_seed(seed_d)
    sigma = hash_g_sigma(d)
    rows = []
    nonce = 0
    for _ in range(K):
        rows.append(sample_poly_cbd2(_prf(sigma, nonce)))
        nonce += 1
    for _ in range(K):
        rows.append(sample_poly_cbd2(_prf(sigma, nonce)))
        nonce += 1
    return np.stack(rows)


build_src_shake128_shim = build_src
build_src_shake256 = build_src
