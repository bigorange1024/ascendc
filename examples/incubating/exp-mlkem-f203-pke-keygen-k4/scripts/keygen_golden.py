#!/usr/bin/env python3
# @probe exp-mlkem-f203-pke-keygen-k4
# @file scripts/keygen_golden.py
# @layer script
# @role Host golden：与设备生产 I/O 对齐的 ek_pke/dk_pke 期望。 / Production golden generator.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify run.sh 编译前生成 input/；KEYGEN_VERIFY=1 时参与对拍。

# coding=utf-8
"""Alg.13 全链 KeyGen golden 组装（仅 Host 黑盒，源码自包含于本探针目录）。"""
from __future__ import annotations

import hashlib
import importlib.util
import os
import struct
import sys
from pathlib import Path
from typing import Any

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "scripts"
PREP_SCRIPTS = SCRIPTS / "prep"
COMPUTE_SCRIPTS = SCRIPTS / "compute"
FIPS203_SE = PREP_SCRIPTS / "fips203_se_sample"

for p in (FIPS203_SE, PREP_SCRIPTS, COMPUTE_SCRIPTS):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))

from golden_se_sampling import build_src, derand_bytes_from_seed  # noqa: E402

_alg7_spec = importlib.util.spec_from_file_location("alg7_gen_data", PREP_SCRIPTS / "gen_data.py")
alg7_gen = importlib.util.module_from_spec(_alg7_spec)
assert _alg7_spec.loader is not None
_alg7_spec.loader.exec_module(alg7_gen)

_v2_spec = importlib.util.spec_from_file_location("v2_gen_data", COMPUTE_SCRIPTS / "gen_data.py")
v2_gen = importlib.util.module_from_spec(_v2_spec)
assert _v2_spec.loader is not None
_v2_spec.loader.exec_module(v2_gen)

from alg7_geom import XOF_BYTES  # noqa: E402

KYBER_K = 4
KYBER_N = 256
AHAT_POLYS = KYBER_K * KYBER_K
EK_POLYVEC_BYTES = KYBER_K * 384
RHO_BYTES = 32
EK_PKE_BYTES = EK_POLYVEC_BYTES + RHO_BYTES
DK_PKE_BYTES = EK_POLYVEC_BYTES


def hash_g_rho_sigma(d: bytes) -> tuple[bytes, bytes]:
    buf = hashlib.sha3_512(d + bytes([KYBER_K & 0xFF])).digest()
    return buf[:32], buf[32:64]


def a_hat_offset(p: int, j: int) -> int:
    return (p * KYBER_K + j) * KYBER_N


def build_a_hat(rho: bytes) -> np.ndarray:
    a_hat = np.empty(AHAT_POLYS * KYBER_N, dtype=np.int32)
    for p in range(KYBER_K):
        for j in range(KYBER_K):
            seed = rho + bytes([j & 0xFF, p & 0xFF])
            xof = alg7_gen.shake128_squeeze(seed, XOF_BYTES)
            d1, d2 = alg7_gen.unpack_d12_from_xof(xof)
            poly = alg7_gen.rej_scalar_from_d12(d1, d2)
            off = a_hat_offset(p, j)
            a_hat[off : off + KYBER_N] = poly
    return a_hat.reshape(AHAT_POLYS, KYBER_N)


def build_lines_16_20(a_hat: np.ndarray, src: np.ndarray, mix_pass: int = 0) -> dict[str, Any]:
    """行 16–20：复用本目录 compute/gen_data 与设备同构的 NTT + hat + ByteEncode golden。"""
    lut = v2_gen.load_lut_t_i8()
    lut_even = v2_gen.lut_planar_stacked(lut, even=True)
    lut_odd = v2_gen.lut_planar_stacked(lut, even=False)

    s_poly = src[: v2_gen.K_S]
    e_poly = src[v2_gen.K_S :]
    s0_ref = v2_gen.encode_2s1e_s0(s_poly, e_poly)
    c_lo_e, c_lo_o, c_hi_e, c_hi_o = v2_gen.mat_c_tmp_golden(s0_ref, lut)
    mat_c_ref = v2_gen.pack_mat_c_planar(c_lo_e, c_lo_o, c_hi_e, c_hi_o)
    dst_ref = v2_gen.golden_dst_from_planar(mat_c_ref)
    s_hat, e_hat = v2_gen.extract_s_e_hat(dst_ref)
    t_hat_dot = v2_gen.golden_t_hat_dot_c(a_hat, s_hat)
    t_hat = v2_gen.golden_t_hat_c(a_hat, s_hat, e_hat)
    ek_polyvec = v2_gen.golden_byte_encode_polyvec(t_hat)
    sk_polyvec = v2_gen.golden_byte_encode_polyvec(s_hat)

    tiling = np.zeros(16, dtype=np.int32)
    tiling[:3] = [KYBER_N, v2_gen.K_S, mix_pass]

    return {
        "lut_even": lut_even,
        "lut_odd": lut_odd,
        "tiling": tiling,
        "s0": s0_ref,
        "mat_c": mat_c_ref,
        "dst": dst_ref,
        "t_hat_dot": t_hat_dot,
        "t_hat": t_hat,
        "ek_polyvec": ek_polyvec,
        "sk_polyvec": sk_polyvec,
        "s_hat": s_hat,
        "e_hat": e_hat,
        "mix_pass": mix_pass,
    }


def build_full_keygen(seed_d: int, mix_pass: int = 0) -> dict[str, Any]:
    os.environ["FIPS203_PRF_BACKEND"] = "shake256"
    d = derand_bytes_from_seed(seed_d)
    rho, sigma = hash_g_rho_sigma(d)
    a_hat = build_a_hat(rho)
    src = build_src(seed_d)
    mid = build_lines_16_20(a_hat, src, mix_pass=mix_pass)
    ek_pke = np.concatenate([mid["ek_polyvec"], np.frombuffer(rho, dtype=np.uint8)])
    return {
        "seed_d": seed_d,
        "d": d,
        "rho": rho,
        "sigma": sigma,
        "a_hat": a_hat,
        "src": src,
        "ek_pke": ek_pke,
        "dk_pke": mid["sk_polyvec"],
        **mid,
    }


def write_keygen_bins(root: Path, kg: dict[str, Any]) -> None:
    inp = root / "input"
    out = root / "output"
    inp.mkdir(exist_ok=True)
    out.mkdir(exist_ok=True)

    (inp / "seed_d.bin").write_bytes(struct.pack("<I", kg["seed_d"]))
    kg["lut_even"].tofile(inp / "lut_even_stacked.bin")
    kg["lut_odd"].tofile(inp / "lut_odd_stacked.bin")
    kg["src"].tofile(inp / "src.bin")
    kg["a_hat"].tofile(inp / "a_hat.bin")
    kg["tiling"].tofile(inp / "tiling.bin")

    (out / "golden_rho.bin").write_bytes(kg["rho"])
    (out / "golden_sigma.bin").write_bytes(kg["sigma"])
    kg["a_hat"].tofile(out / "golden_a_hat.bin")
    kg["src"].tofile(out / "golden_src.bin")
    kg["s0"].tofile(out / "golden_s0.bin")
    kg["mat_c"].tofile(out / "golden_mat_c.bin")
    kg["dst"].tofile(out / "golden.bin")
    kg["t_hat_dot"].tofile(out / "golden_t_hat_dot.bin")
    kg["t_hat"].tofile(out / "golden_t_hat.bin")
    kg["ek_polyvec"].tofile(out / "golden_ek_polyvec.bin")
    kg["sk_polyvec"].tofile(out / "golden_sk_polyvec.bin")
    kg["ek_pke"].tofile(out / "golden_ek_pke.bin")
    kg["dk_pke"].tofile(out / "golden_dk_pke.bin")

    kg["ek_polyvec"].tofile(inp / "ek_polyvec.bin")
    (inp / "rho.bin").write_bytes(kg["rho"])
