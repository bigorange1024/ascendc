#!/usr/bin/env python3
# @probe pass-fix-f203-alg13-device-keygen-k3
# @file scripts/compute/gen_data.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `gen_data.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1184B) + dk_pke.bin (1152B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps. compute 子树可单独跑中间 bin（调试）。 / Compute subtree debug bins optional.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify run.sh 编译前生成 input/；KEYGEN_VERIFY=1 时参与对拍。

# coding=utf-8
"""
本文件在 KeyGen 流水线中的位置：Host：compute 段 golden / 对拍脚本。
对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
文件：scripts/compute/gen_data.py
"""
"""
gen_data.py — pass-fix-f203-alg13-device-keygen-k3 的 compute golden 生成器。

## 数据契约（与设备 mmad_custom / aiv_func 必须一致）

  src [6,256]   : 行 0..2 为 ŝ，行 3..5 为 ê（polyvec6，禁止 pad 到 8）
  mat_c [48,128]: 平面布局，行 = planar_row(slot, limb, half)
  dst [6,256]   : NTT 后 ŝ̂/ê̂，行 0..2 共享给行 18，行 3..5 作 ê̂
  t_hat, ek, sk : Alg.13 行 18–20（C ref 标量 mod，与 F203_MOD_VARIANT 解耦）

## NTT golden 链（与设备同构，非逐 poly MlkemNtt）

  src → encode_2s1e_s0 → mat_c_tmp_golden → pack_mat_c_planar
      → golden_dst_from_planar (merge_planar_poly + stage31_mod)

## 行 18–20 golden

  golden_t_hat_dot : hat_inner_product_dot（Σ_j，无 ê）
  golden_t_hat     : hat_inner_product_add（Σ_j + ê，一次 mod）
  golden_ek/sk     : byte_encode12_ref

## 环境变量

  NTTS2S1E_MIX_PASS / TAG5T_MIX_PASS — tiling.mixPass
  NTTS2S1E_E_POLY_SEED — e 基多项式种子（默认 43）
  SEED_AHAT=20260615 — a_hat 随机种子

详见：IMPLEMENTATION_REFERENCE.md §2
"""
import ctypes
import os
import re
import subprocess
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, "../.."))
_MLKEM_REF = _SCRIPT_DIR
_NTT_LUT_HDR = os.path.normpath(
    os.path.join(_CASE_DIR, "thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h")
)
sys.path.insert(0, _MLKEM_REF)

import merged_kyber_fixed_poly  # noqa: E402
from mlkem_ref import stage31_mod  # noqa: E402

N = 256
HALF_N = N // 2
K_S = 3          # ŝ 多项式个数（ML-KEM-768 k=3）
K_E = 3          # ê 多项式个数（ML-KEM-768 k=3）
K_E_PER_AIV = 3  # NTT 阶段按 polyvec6 连续 3+3 分片；行 18 另按 p=2+1 分片
K_HAT = 3        # t_hat / ek 行数
K_KK = 9         # a_hat 行数（行 18：3×3）
K_PLANAR_SLOTS = K_S + K_E        # mat_c slot 数 = 6
LIMBS = 4        # hh,lh,hl,ll
MAT_C_PLANAR_ROWS = K_PLANAR_SLOTS * LIMBS * 2  # 48 = 6×4×2(half)
M_ROWS = 2 * (K_S + K_E)  # S0 行数：[HI₆, LO₆]
LIMB_MASK = 0x3F
LIMB_BITS = 6
Q = 3329
SEED_AHAT = 20260615
# C ref 行 18 golden：固定标量 int64 floor mod（与设备 F203_MOD_VARIANT 解耦）
HAT_GOLDEN_MOD_VARIANT = 0
# 与 mod_config.hpp 同步，仅用于 gen_data 日志
F203_MOD_VARIANT = 1
POLYVEC_BYTES = K_HAT * 384
RHO_BYTES = 32
EK_PKE_BYTES = POLYVEC_BYTES + RHO_BYTES
SEED_RHO = 20260620

S0_ROW_ALL = 0
PLANAR_SLOT_ALL = 0
K_DST = K_S + K_E


# 本函数为 KeyGen 流水线组件 `load_lut_t_i8`（详见 STATUS/customspec）。
def load_lut_t_i8() -> np.ndarray:
    with open(_NTT_LUT_HDR, encoding="utf-8") as f:
        txt = f.read()
    anchor = "kMlkemLimb6Ntt_T_i8"
    i0 = txt.index(anchor)
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    body = txt[i1 + 1 : i2]
    nums = [int(x) for x in re.findall(r"-?\d+", body)]
    expect = N * 512
    if len(nums) != expect:
        raise SystemExit(f"[gen_data] LUT size {len(nums)} != {expect}")
    return np.array(nums, dtype=np.int8).reshape(N, 512)


# 本函数为 KeyGen 流水线组件 `lut_planar_stacked`（详见 STATUS/customspec）。
def lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def planar_row(slot: int, limb: int, half: int) -> int:
    """与 tiling.h / AivK6PackMatCPlanar 一致：half*24 + slot*4 + limb。"""
    return half * (K_PLANAR_SLOTS * LIMBS) + slot * LIMBS + limb


def encode_polyvec6_s0(src: np.ndarray) -> np.ndarray:
    """
    Stage1 golden：src[6,256] → S0[12,256]。

    上半 6 行写 hi=(v>>6)&63，下半 6 行写 lo=v-hi*64；与设备 `AivK6Split`
    的真实 polyvec6 几何一致，不复制 k4 的双 ŝ bank，也不补零到 8 行。
    """
    s0 = np.zeros((M_ROWS, N), dtype=np.int8)
    if src.shape != (K_DST, N):
        raise ValueError(f"src shape {src.shape} != ({K_DST},{N})")
    for lp in range(K_DST):
        for r in range(N):
            v = int(src[lp, r]) % Q
            hi = (v >> LIMB_BITS) & LIMB_MASK
            s0[S0_ROW_ALL + lp, r] = hi
            s0[K_DST + lp, r] = v - hi * (1 << LIMB_BITS)
    return s0


def encode_2s1e_s0(s_poly: np.ndarray, e_poly: np.ndarray) -> np.ndarray:
    """兼容旧调用名：把 ŝ‖ê 拼成真实 polyvec6 后走 encode_polyvec6_s0。"""
    return encode_polyvec6_s0(np.vstack([s_poly, e_poly]))


def mat_c_tmp_golden(s0: np.ndarray, lut: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    le = lut[:, 0:N:2]
    lo = lut[:, 1:N:2]
    he = lut[:, N:512:2]
    ho = lut[:, N + 1 : 512 : 2]
    c_lo_even = (s0.astype(np.int32) @ le.astype(np.int32)).astype(np.int32)
    c_lo_odd = (s0.astype(np.int32) @ lo.astype(np.int32)).astype(np.int32)
    c_hi_even = (s0.astype(np.int32) @ he.astype(np.int32)).astype(np.int32)
    c_hi_odd = (s0.astype(np.int32) @ ho.astype(np.int32)).astype(np.int32)
    return c_lo_even, c_lo_odd, c_hi_even, c_hi_odd


# 本函数为 KeyGen 流水线组件 `pack_bank_planar`（详见 STATUS/customspec）。
def pack_bank_planar(
    c_lo_even: np.ndarray,
    c_lo_odd: np.ndarray,
    c_hi_even: np.ndarray,
    c_hi_odd: np.ndarray,
    row_base: int,
    slot_base: int,
    k_polys: int,
    out: np.ndarray,
) -> None:
    for lp in range(k_polys):
        hi_r = row_base + lp
        lo_r = row_base + k_polys + lp
        slot = slot_base + lp
        out[planar_row(slot, 0, 0), :] = c_lo_even[hi_r, :]
        out[planar_row(slot, 1, 0), :] = c_lo_odd[hi_r, :]
        out[planar_row(slot, 2, 0), :] = c_lo_even[lo_r, :]
        out[planar_row(slot, 3, 0), :] = c_lo_odd[lo_r, :]
        out[planar_row(slot, 0, 1), :] = c_hi_even[hi_r, :]
        out[planar_row(slot, 1, 1), :] = c_hi_odd[hi_r, :]
        out[planar_row(slot, 2, 1), :] = c_hi_even[lo_r, :]
        out[planar_row(slot, 3, 1), :] = c_hi_odd[lo_r, :]


# 本函数为 KeyGen 流水线组件 `pack_mat_c_planar`（详见 STATUS/customspec）。
def pack_mat_c_planar(
    c_lo_even: np.ndarray,
    c_lo_odd: np.ndarray,
    c_hi_even: np.ndarray,
    c_hi_odd: np.ndarray,
) -> np.ndarray:
    out = np.zeros((MAT_C_PLANAR_ROWS, HALF_N), dtype=np.int32)
    pack_bank_planar(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, S0_ROW_ALL, PLANAR_SLOT_ALL, K_DST, out)
    return out


# 本函数为 KeyGen 流水线组件 `merge_planar_poly`（详见 STATUS/customspec）。
def merge_planar_poly(mat_planar: np.ndarray, slot: int) -> np.ndarray:
    """Stage3 golden：平面 8 行 → raw_lo/raw_hi → stage31_mod。与 ntt_vec.hpp 同公式。"""
    hh = mat_planar[planar_row(slot, 0, 0)].astype(np.int64)
    lh = mat_planar[planar_row(slot, 1, 0)].astype(np.int64)
    hl = mat_planar[planar_row(slot, 2, 0)].astype(np.int64)
    ll = mat_planar[planar_row(slot, 3, 0)].astype(np.int64)
    raw_lo = hh * 4096 + (hl + lh) * 64 + ll
    hh = mat_planar[planar_row(slot, 0, 1)].astype(np.int64)
    lh = mat_planar[planar_row(slot, 1, 1)].astype(np.int64)
    hl = mat_planar[planar_row(slot, 2, 1)].astype(np.int64)
    ll = mat_planar[planar_row(slot, 3, 1)].astype(np.int64)
    raw_hi = hh * 4096 + (hl + lh) * 64 + ll
    out = np.zeros(N, dtype=np.int32)
    out[:HALF_N] = stage31_mod(raw_lo.astype(np.int32))
    out[HALF_N:] = stage31_mod(raw_hi.astype(np.int32))
    return out


# 本函数为 KeyGen 流水线组件 `golden_dst_from_planar`（详见 STATUS/customspec）。
def golden_dst_from_planar(mat_planar: np.ndarray) -> np.ndarray:
    dst = np.zeros((K_DST, N), dtype=np.int32)
    for slot in range(K_PLANAR_SLOTS):
        dst[slot] = merge_planar_poly(mat_planar, slot)
    return dst


def extract_s_e_hat(dst: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    s_hat = dst[:K_S].copy()
    e_hat = dst[K_S : K_S + K_E].copy()
    return s_hat, e_hat


def _load_hat_ref_lib() -> ctypes.CDLL:
    build_dir = os.path.join(_CASE_DIR, "build_ref")
    os.makedirs(build_dir, exist_ok=True)
    so_path = os.path.join(build_dir, "libhat_inner_product_ref.so")
    src_c = os.path.join(_CASE_DIR, "compute", "hat_inner_product_ref.c")
    if not os.path.isfile(so_path) or os.path.getmtime(so_path) < os.path.getmtime(src_c):
        subprocess.run(["gcc", "-shared", "-fPIC", "-O2", src_c, "-o", so_path], check=True, cwd=_CASE_DIR)
    return ctypes.CDLL(so_path)


# 本函数为 KeyGen 流水线组件 `_load_byte_encode_ref_lib`（详见 STATUS/customspec）。
def _load_byte_encode_ref_lib() -> ctypes.CDLL:
    build_dir = os.path.join(_CASE_DIR, "build_ref")
    os.makedirs(build_dir, exist_ok=True)
    so_path = os.path.join(build_dir, "libbyte_encode12_ref.so")
    src_c = os.path.join(_CASE_DIR, "compute", "byte_encode12_ref.c")
    if not os.path.isfile(so_path) or os.path.getmtime(so_path) < os.path.getmtime(src_c):
        subprocess.run(["gcc", "-shared", "-fPIC", "-O2", src_c, "-o", so_path], check=True, cwd=_CASE_DIR)
    return ctypes.CDLL(so_path)


def golden_byte_encode_polyvec(polys: np.ndarray) -> np.ndarray:
    lib = _load_byte_encode_ref_lib()
    lib.polyvec_byte_encode12_ref.argtypes = [
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.c_int,
        ctypes.c_int,
    ]
    lib.polyvec_byte_encode12_ref.restype = None
    out = np.zeros(POLYVEC_BYTES, dtype=np.uint8)
    lib.polyvec_byte_encode12_ref(
        out.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8)),
        polys.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        K_HAT,
        N,
    )
    return out


# 本函数为 KeyGen 流水线组件 `golden_t_hat_dot_c`（详见 STATUS/customspec）。
def golden_t_hat_dot_c(a_hat: np.ndarray, s_hat: np.ndarray) -> np.ndarray:
    lib = _load_hat_ref_lib()
    lib.hat_inner_product_dot.argtypes = [
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.c_int,
    ]
    lib.hat_inner_product_dot.restype = None
    out = np.zeros((K_HAT, N), dtype=np.int32)
    lib.hat_inner_product_dot(
        a_hat.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        s_hat.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        out.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        HAT_GOLDEN_MOD_VARIANT,
    )
    return out


# 本函数为 KeyGen 流水线组件 `golden_t_hat_c`（详见 STATUS/customspec）。
def golden_t_hat_c(a_hat: np.ndarray, s_hat: np.ndarray, e_hat: np.ndarray) -> np.ndarray:
    lib = _load_hat_ref_lib()
    lib.hat_inner_product_add.argtypes = [
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.c_int,
    ]
    lib.hat_inner_product_add.restype = None
    out = np.zeros((K_HAT, N), dtype=np.int32)
    lib.hat_inner_product_add(
        a_hat.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        s_hat.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        e_hat.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        out.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        HAT_GOLDEN_MOD_VARIANT,
    )
    return out


# 本函数为 KeyGen 流水线组件 `build_src_se`（详见 STATUS/customspec）。
def build_src_se() -> np.ndarray:
    """
    构造 host src [6,256]。

    行 0..2：同一 FIXED_POLY ×3（调试路径）；
    行 3..5：基 e 上 (e+0..2) mod Q，对应 ML-KEM-768 三条 ê。
    """
    s = merged_kyber_fixed_poly.FIXED_POLY.copy()
    e_seed = int(os.environ.get("NTTS2S1E_E_POLY_SEED", "43"))
    e = np.random.default_rng(e_seed).integers(0, Q, size=N, dtype=np.int32)
    s_block = np.tile(s.reshape(1, -1), (K_S, 1))
    e_block = np.zeros((K_E, N), dtype=np.int32)
    for p in range(K_E):
        e_block[p] = (e + p) % Q
    return np.vstack([s_block, e_block])


def gen_tiling(mix_pass: int = 0) -> None:
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    payload = np.array([N, K_S, mix_pass], dtype=np.int32)
    buf = np.zeros(16, dtype=np.int32)
    buf[:3] = payload
    buf.tofile("./input/tiling.bin")


if __name__ == "__main__":
    # 生成 input/ + output/golden_*；顺序与 IMPLEMENTATION_REFERENCE.md §2.3 一致
    mix_pass = int(os.environ.get("NTTS2S1E_MIX_PASS", os.environ.get("TAG5T_MIX_PASS", "0")))
    gen_tiling(mix_pass)

    src = build_src_se()
    rng = np.random.default_rng(SEED_AHAT)
    a_hat = rng.integers(0, Q, size=(K_KK, N), dtype=np.int32)

    lut = load_lut_t_i8()
    lut_even_stacked = lut_planar_stacked(lut, even=True)
    lut_odd_stacked = lut_planar_stacked(lut, even=False)

    lut_even_stacked.tofile("./input/lut_even_stacked.bin")
    lut_odd_stacked.tofile("./input/lut_odd_stacked.bin")
    src.tofile("./input/src.bin")
    a_hat.tofile("./input/a_hat.bin")

    s_poly = src[:K_S]
    e_poly = src[K_S:]
    s0_ref = encode_2s1e_s0(s_poly, e_poly)
    c_lo_e, c_lo_o, c_hi_e, c_hi_o = mat_c_tmp_golden(s0_ref, lut)
    mat_c_ref = pack_mat_c_planar(c_lo_e, c_lo_o, c_hi_e, c_hi_o)
    dst_ref = golden_dst_from_planar(mat_c_ref)
    s_hat, e_hat = extract_s_e_hat(dst_ref)
    t_hat_dot_ref = golden_t_hat_dot_c(a_hat, s_hat)
    t_hat_ref = golden_t_hat_c(a_hat, s_hat, e_hat)
    ek_ref = golden_byte_encode_polyvec(t_hat_ref)
    sk_ref = golden_byte_encode_polyvec(s_hat)
    rho_ref = np.random.default_rng(SEED_RHO).integers(0, 256, size=RHO_BYTES, dtype=np.uint8)
    ek_pke_ref = np.concatenate([ek_ref, rho_ref])

    rho_ref.tofile("./input/rho.bin")
    rho_ref.tofile("./output/golden_rho.bin")
    ek_pke_ref.tofile("./output/golden_ek_pke.bin")

    s0_ref.tofile("./output/golden_s0.bin")
    mat_c_ref.tofile("./output/golden_mat_c.bin")
    dst_ref.tofile("./output/golden.bin")
    t_hat_dot_ref.tofile("./output/golden_t_hat_dot.bin")
    t_hat_ref.tofile("./output/golden_t_hat.bin")
    ek_ref.tofile("./output/golden_ek_polyvec.bin")
    sk_ref.tofile("./output/golden_sk_polyvec.bin")

    print(f"[gen_data] src shape ({K_S + K_E},{N}) mixPass={mix_pass}")
    print(f"[gen_data] golden dst {dst_ref.shape} mat_c_planar {mat_c_ref.shape} a_hat seed={SEED_AHAT}")
    print(f"[gen_data] t_hat golden=C scalar mod; device F203_MOD_VARIANT={F203_MOD_VARIANT}")
    print(f"[gen_data] rho {RHO_BYTES}B seed={SEED_RHO} ek_pke={EK_PKE_BYTES}B (=ek_polyvec||rho)")
