#!/usr/bin/env python3
# @probe exp-fips203-mlkem-pke-keygen-k2
# @file scripts/prep/fips203_se_sample/golden_se_sampling.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `golden_se_sampling.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify run.sh 编译前生成 input/；KEYGEN_VERIFY=1 时参与对拍。

# coding=utf-8
"""
本文件在 KeyGen 流水线中的位置：Host：prep 段 golden / ROM 生成脚本。
对齐：FIPS 203 Alg.13 / ML-KEM-512（k=2）。
与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
文件：scripts/prep/fips203_se_sample/golden_se_sampling.py
"""

from __future__ import annotations

"""Python golden for Alg.13 行 8–15 — 与 fips203_prf / fips203_se_sample.c 同语义。"""

import hashlib
import os

import numpy as np

Q = 3329
N = 256
K = 2
ETA = 3
SEED_D_DEFAULT = 20260619


def derand_bytes_from_seed(seed_d: int) -> bytes:
    """按 D13 锁定域分离从整数 SEED_D 派生 32B d。"""
    msg = f"exp-mlkem-f203-2s1e-k2:SEED_D={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


# 本函数为 KeyGen 流水线组件 `hash_g_sigma`（详见 STATUS/customspec）。
def hash_g_sigma(d: bytes) -> bytes:
    buf = hashlib.sha3_512(d + bytes([K & 0xFF])).digest()
    return buf[32:64]


def prf_shake128(sigma: bytes, nonce: int) -> bytes:
    outlen = ETA * N // 4
    return hashlib.shake_128(sigma + bytes([nonce & 0xFF])).digest(outlen)


def prf_shake256(sigma: bytes, nonce: int) -> bytes:
    outlen = ETA * N // 4
    return hashlib.shake_256(sigma + bytes([nonce & 0xFF])).digest(outlen)


def _prf(sigma: bytes, nonce: int) -> bytes:
    backend = os.environ.get("FIPS203_PRF_BACKEND", "shake128").lower()
    if backend == "shake256":
        return prf_shake256(sigma, nonce)
    return prf_shake128(sigma, nonce)


# 本函数为 KeyGen 流水线组件 `_load32_le`（详见 STATUS/customspec）。
def _load32_le(buf: bytes, off: int) -> int:
    return int(buf[off]) | (int(buf[off + 1]) << 8) | (int(buf[off + 2]) << 16) | (int(buf[off + 3]) << 24)


def sample_poly_cbd3(buf: bytes) -> np.ndarray:
    coeffs = np.zeros(N, dtype=np.int32)
    for i in range(N // 4):
        t = int(buf[3 * i]) | (int(buf[3 * i + 1]) << 8) | (int(buf[3 * i + 2]) << 16)
        d = (t & 0x00249249) + ((t >> 1) & 0x00249249) + ((t >> 2) & 0x00249249)
        for j in range(4):
            a = (d >> (6 * j + 0)) & 0x7
            b = (d >> (6 * j + 3)) & 0x7
            c = a - b
            if c < 0:
                c += Q
            coeffs[4 * i + j] = c % Q
    return coeffs


# 本函数为 KeyGen 流水线组件 `build_src`（详见 STATUS/customspec）。
def build_src(seed_d: int | None = None) -> np.ndarray:
    if seed_d is None:
        seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    d = derand_bytes_from_seed(seed_d)
    sigma = hash_g_sigma(d)
    rows = []
    nonce = 0
    for _ in range(K):
        # Alg.13 行 8–11：ML-KEM-512 的 s 使用 η1=3；本 D13 按参数卡锁定 e 同用 η=3。
        rows.append(sample_poly_cbd3(_prf(sigma, nonce)))
        nonce += 1
    for _ in range(K):
        rows.append(sample_poly_cbd3(_prf(sigma, nonce)))
        nonce += 1
    return np.stack(rows)


build_src_shake128_shim = build_src
build_src_shake256 = build_src
