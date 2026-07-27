#!/usr/bin/env python3
# @probe exp-fips203-mlkem-pke-keygen-k2
# @file scripts/keygen_golden.py
# @layer script
# @role Host golden：与设备生产 I/O 对齐的 ek_pke/dk_pke 期望。 / Production golden generator.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify run.sh 编译前生成 input/；KEYGEN_VERIFY=1 时参与对拍。

# coding=utf-8
"""
Alg.13 全链 KeyGen Host golden 组装（ML-KEM-512，k=2）。

## 流水线位置
黑盒 oracle：复用本目录 prep/compute 子脚本生成 Â、ŝ/ê、NTT+hat+ByteEncode 期望，
供 `scripts/gen_data.py` 写 bin；**不是** AscendC 实现规格。

## 与设备关系
验收仅要求设备 `ek_pke`/`dk_pke` 与本模块输出 I/O 等价。
"""
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

# 将 prep/compute golden 子模块加入 path（自包含，不依赖 ascendc-tests）
for p in (FIPS203_SE, PREP_SCRIPTS, COMPUTE_SCRIPTS):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))

from golden_se_sampling import build_src, derand_bytes_from_seed  # noqa: E402

# 动态加载 prep/alg7 与 compute 的 gen_data，避免包名冲突
_alg7_spec = importlib.util.spec_from_file_location("alg7_gen_data", PREP_SCRIPTS / "gen_data.py")
alg7_gen = importlib.util.module_from_spec(_alg7_spec)
assert _alg7_spec.loader is not None
_alg7_spec.loader.exec_module(alg7_gen)

_v2_spec = importlib.util.spec_from_file_location("v2_gen_data", COMPUTE_SCRIPTS / "gen_data.py")
v2_gen = importlib.util.module_from_spec(_v2_spec)
assert _v2_spec.loader is not None
_v2_spec.loader.exec_module(v2_gen)

from alg7_geom import XOF_BYTES  # noqa: E402

KYBER_K = 2
KYBER_N = 256
AHAT_POLYS = KYBER_K * KYBER_K
EK_POLYVEC_BYTES = KYBER_K * 384
RHO_BYTES = 32
EK_PKE_BYTES = EK_POLYVEC_BYTES + RHO_BYTES
DK_PKE_BYTES = EK_POLYVEC_BYTES


# 本函数为 KeyGen 流水线组件 `hash_g_rho_sigma`（详见 STATUS/customspec）。
def hash_g_rho_sigma(d: bytes) -> tuple[bytes, bytes]:
    """
    Alg.13 行 1–2：G(d‖k) = SHA3-512 → (ρ, σ) 各 32B。

    参数:
        d: 32B 确定性种子字节（由 derand_bytes_from_seed 得到）
    返回:
        (rho, sigma)
    """
    buf = hashlib.sha3_512(d + bytes([KYBER_K & 0xFF])).digest()
    return buf[:32], buf[32:64]


def a_hat_offset(p: int, j: int) -> int:
    """Â 矩阵展平行偏移：行主序 poly 索引 (p,j) → 元素起点。"""
    return (p * KYBER_K + j) * KYBER_N


def build_a_hat(rho: bytes) -> np.ndarray:
    """
    Alg.7 SampleNTT：对 k×k 个 poly 做 SHAKE128 + 12-bit 解包 + 拒绝采样。

    参数:
        rho: 32B 公钥种子
    返回:
        shape [4,256] int32 的 Â（与设备 a_hat_gm 布局一致）
    """
    a_hat = np.empty(AHAT_POLYS * KYBER_N, dtype=np.int32)
    for p in range(KYBER_K):
        for j in range(KYBER_K):
            # FIPS：XOF 种子 = ρ ‖ j ‖ p（注意 j 在前）
            seed = rho + bytes([j & 0xFF, p & 0xFF])
            xof = alg7_gen.shake128_squeeze(seed, XOF_BYTES)
            d1, d2 = alg7_gen.unpack_d12_from_xof(xof)
            poly = alg7_gen.rej_scalar_from_d12(d1, d2)
            off = a_hat_offset(p, j)
            a_hat[off : off + KYBER_N] = poly
    return a_hat.reshape(AHAT_POLYS, KYBER_N)


def build_lines_16_20(a_hat: np.ndarray, src: np.ndarray, mix_pass: int = 0) -> dict[str, Any]:
    """
    行 16–20 Host oracle：NTT(ŝ/ê) → t̂=Â∘ŝ+ê → ByteEncode₁₂。

    复用 compute/gen_data 几何，与设备 I/O 对齐；**不**要求 AscendC 逐步同构。

    参数:
        a_hat: [4,256] int32
        src: [4,256] int32（前 k 行 ŝ，后 k 行 ê）
        mix_pass: 写入 tiling 调试字段；生产为 0
    返回:
        含 lut_even/odd、dst、t_hat、ek/sk_polyvec 等中间与最终编码
    """
    lut = v2_gen.load_lut_t_i8()
    lut_even = v2_gen.lut_planar_stacked(lut, even=True)
    lut_odd = v2_gen.lut_planar_stacked(lut, even=False)

    # 拆 ŝ/ê → Stage1 S0 参考 → 四路 mat_c 临时 → 平面 → NTT dst
    s_poly = src[: v2_gen.K_S]
    e_poly = src[v2_gen.K_S :]
    s0_ref = v2_gen.encode_2s1e_s0(s_poly, e_poly)
    c_lo_e, c_lo_o, c_hi_e, c_hi_o = v2_gen.mat_c_tmp_golden(s0_ref, lut)
    mat_c_ref = v2_gen.pack_mat_c_planar(c_lo_e, c_lo_o, c_hi_e, c_hi_o)
    dst_ref = v2_gen.golden_dst_from_planar(mat_c_ref)
    s_hat, e_hat = v2_gen.extract_s_e_hat(dst_ref)
    # 行 18：点积 Â∘ŝ；完整 t̂ 再加 ê
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


# 本函数为 KeyGen 流水线组件 `build_full_keygen`（详见 STATUS/customspec）。
def build_full_keygen(seed_d: int, mix_pass: int = 0) -> dict[str, Any]:
    """
    全链 KeyGen Host：seed_d → (ρ,σ,Â,src) → 行16–20 → ek_pke/dk_pke。

    参数:
        seed_d: uint32 种子（与 input/seed_d.bin 一致）
        mix_pass: 调试用；生产 0
    返回:
        字典，含生产输出 ek_pke/dk_pke 及调试中间态
    """
    os.environ["FIPS203_PRF_BACKEND"] = "shake256"
    d = derand_bytes_from_seed(seed_d)
    rho, sigma = hash_g_rho_sigma(d)
    a_hat = build_a_hat(rho)
    src = build_src(seed_d)
    mid = build_lines_16_20(a_hat, src, mix_pass=mix_pass)
    # 行 21：ek_PKE = ek_polyvec ‖ ρ
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


# 本函数为 KeyGen 流水线组件 `write_keygen_bins`（详见 STATUS/customspec）。
def write_keygen_bins(root: Path, kg: dict[str, Any]) -> None:
    """
    将 golden 字典落盘到 input/ 与 output/。

    生产设备仅需 input 的 seed_d + lut_even/odd；其余 bin 供调试/分段对拍。
    """
    inp = root / "input"
    out = root / "output"
    inp.mkdir(exist_ok=True)
    out.mkdir(exist_ok=True)

    # --- 生产输入 ---
    (inp / "seed_d.bin").write_bytes(struct.pack("<I", kg["seed_d"]))
    kg["lut_even"].tofile(inp / "lut_even_stacked.bin")
    kg["lut_odd"].tofile(inp / "lut_odd_stacked.bin")
    # --- 调试/分段输入（设备默认不读）---
    kg["src"].tofile(inp / "src.bin")
    kg["a_hat"].tofile(inp / "a_hat.bin")
    kg["tiling"].tofile(inp / "tiling.bin")

    # --- golden 输出（对拍用）---
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
