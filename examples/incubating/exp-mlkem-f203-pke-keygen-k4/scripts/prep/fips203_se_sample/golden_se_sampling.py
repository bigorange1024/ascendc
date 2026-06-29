#!/usr/bin/env python3
# @probe exp-mlkem-f203-pke-keygen-k4
# @file scripts/prep/fips203_se_sample/golden_se_sampling.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `golden_se_sampling.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify run.sh 编译前生成 input/；KEYGEN_VERIFY=1 时参与对拍。

# coding=utf-8
"""Python golden for Alg.13 行 8–15 — 与 fips203_prf / fips203_se_sample.c 同语义。"""
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
    msg = f"exp-mlkem-f203-2s1e-k4:SEED_D={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


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


def build_src(seed_d: int | None = None) -> np.ndarray:
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


def load_src_via_c_lib(case_dir: str, seed_d: int | None = None) -> np.ndarray:
    import ctypes
    import subprocess

    if seed_d is None:
        seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    build_dir = os.path.join(case_dir, "build_ref")
    os.makedirs(build_dir, exist_ok=True)
    shared_root = os.path.normpath(os.path.join(case_dir, "../../library/shared/fips203_se_sample"))
    sha3_root = os.path.normpath(os.path.join(case_dir, "../../thirdparty/tiny_sha3"))
    so_path = os.path.join(build_dir, "libfips203_se_sample.so")
    se_files = [
        os.path.join(shared_root, "fips203_prf.c"),
        os.path.join(shared_root, "fips203_se_sample.c"),
        os.path.join(sha3_root, "sha3.c"),
    ]
    newest = max(os.path.getmtime(p) for p in se_files)
    if not os.path.isfile(so_path) or os.path.getmtime(so_path) < newest:
        subprocess.run(
            [
                "gcc",
                "-shared",
                "-fPIC",
                "-O2",
                f"-I{shared_root}",
                f"-I{sha3_root}",
                se_files[0],
                se_files[1],
                se_files[2],
                "-o",
                so_path,
            ],
            check=True,
        )
    lib = ctypes.CDLL(so_path)
    lib.fips203_build_src.argtypes = [ctypes.POINTER(ctypes.c_int32), ctypes.c_uint32]
    lib.fips203_build_src.restype = ctypes.c_int
    buf = np.zeros((2 * K, N), dtype=np.int32)
    rc = lib.fips203_build_src(buf.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)), ctypes.c_uint32(seed_d))
    if rc != 0:
        raise SystemExit(f"[golden_se] C build_src failed rc={rc}")
    return buf
