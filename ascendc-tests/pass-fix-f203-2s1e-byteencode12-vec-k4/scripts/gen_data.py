#!/usr/bin/env python3
# coding=utf-8
"""
@file gen_data.py
@brief ByteEncode₁₂-only 探针：生成 input preset 与 golden ek/sk。

流水线位置：run.sh 编译/跑 kernel 前调用；产出供 main 读取、verify 对拍。
与 golden 关系：本脚本即 golden 生成器——经 v2 Alg.13 链得到 dst/t_hat，再 ByteEncode₁₂。
作用：复用 vec-k4-v2 gen_data 的 NTT→t_hat 链，写出与 mixPass=7 一致的输入契约。

输入契约与 vec-k4-v2 Alg.13 mixPass=7 完全一致：
  input/dst.bin    [12,256] int32 — 同 v2 output/golden.bin / mixPass=7 dst_preset
  input/t_hat.bin  [4,256]  int32 — 同 v2 output/golden_t_hat.bin / t_hat_preset
  output/golden_ek_polyvec.bin — ByteEncode₁₂(t̂)，4×384 B
  output/golden_sk_polyvec.bin — ByteEncode₁₂(ŝ)，4×384 B（ŝ = dst[0:4]）

Golden 链与 pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/scripts/gen_data.py 相同：
  src → NTT → dst → hat_inner_product → t_hat → byte_encode12_ref
"""
import importlib.util
import os
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_V2_GEN = os.path.normpath(
    os.path.join(_SCRIPT_DIR, "..", "..", "pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2", "scripts", "gen_data.py")
)

N = 256
K_S = 4
K_HAT = 4
K_DST = 12
POLYVEC_BYTES = K_HAT * 384


def _load_v2_gen():
    """
    动态加载活跃探针 vec-k4-v2 的 gen_data 模块（复用其 golden 链函数）。

    返回：
        已 exec 的模块对象（含 build_src_se、golden_t_hat_c、golden_byte_encode_polyvec 等）。
    前置条件：
        _V2_GEN 路径存在；否则 SystemExit。
    """
    if not os.path.isfile(_V2_GEN):
        raise SystemExit(f"[gen_data] missing v2 gen_data: {_V2_GEN}")
    spec = importlib.util.spec_from_file_location("v2_gen_data", _V2_GEN)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main() -> None:
    """
    生成本探针全部 input/golden bin。

    流程：
      1) 用 v2 链：src → Stage1/2/3 → dst_ref、t_hat_ref
      2) ByteEncode₁₂(t̂)→ek、ByteEncode₁₂(ŝ)→sk
      3) 落盘 input/dst、t_hat 与 output/golden_*（含调试用 golden_dst/t_hat）

    与设备关系：kernel 只读 dst/t_hat，写出 ek/sk；verify 对拍 golden_ek/sk。
    tiling 不再由 Python 落盘：运行时改由 byte_encode12_tiling.cpp 的 GenerateTiling 生成。
    """
    v2 = _load_v2_gen()
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    # —— 与 v2 相同的随机/LUT/NTT→dst→t_hat 链 ——
    src = v2.build_src_se()
    rng = np.random.default_rng(v2.SEED_AHAT)
    a_hat = rng.integers(0, v2.Q, size=(v2.K_KK, N), dtype=np.int32)

    lut = v2.load_lut_t_i8()
    lut_even_stacked = v2.lut_planar_stacked(lut, even=True)
    lut_odd_stacked = v2.lut_planar_stacked(lut, even=False)

    s_poly = src[: v2.K_S]
    e_poly = src[v2.K_S :]
    s0_ref = v2.encode_2s1e_s0(s_poly, e_poly)
    c_lo_e, c_lo_o, c_hi_e, c_hi_o = v2.mat_c_tmp_golden(s0_ref, lut)
    mat_c_ref = v2.pack_mat_c_planar(c_lo_e, c_lo_o, c_hi_e, c_hi_o)
    dst_ref = v2.golden_dst_from_planar(mat_c_ref)
    s_hat, e_hat = v2.extract_s_e_hat(dst_ref)
    t_hat_ref = v2.golden_t_hat_c(a_hat, s_hat, e_hat)
    # 本探针验收目标：ek/sk 字节流
    ek_ref = v2.golden_byte_encode_polyvec(t_hat_ref)
    sk_ref = v2.golden_byte_encode_polyvec(s_hat)

    # tiling 不再由 Python 落盘：运行时改由 byte_encode12_tiling.cpp 的 GenerateTiling 生成。

    dst_ref.astype(np.int32).tofile(os.path.join(_CASE_DIR, "input", "dst.bin"))
    t_hat_ref.astype(np.int32).tofile(os.path.join(_CASE_DIR, "input", "t_hat.bin"))

    ek_ref.tofile(os.path.join(_CASE_DIR, "output", "golden_ek_polyvec.bin"))
    sk_ref.tofile(os.path.join(_CASE_DIR, "output", "golden_sk_polyvec.bin"))

    dst_ref.tofile(os.path.join(_CASE_DIR, "output", "golden_dst.bin"))
    t_hat_ref.tofile(os.path.join(_CASE_DIR, "output", "golden_t_hat.bin"))

    # 校验 dst 中两份 ŝ 副本一致（aiv0 行 0..3 与 aiv1 行 4..7）
    s_hat_dup = dst_ref[K_S : 2 * K_S]
    print(f"[gen_data] Alg.13 contract: dst {dst_ref.shape} t_hat {t_hat_ref.shape} ek/sk {POLYVEC_BYTES}B each")
    print(f"[gen_data] s_hat dup max_abs_diff={int(np.abs(s_hat.astype(np.int64) - s_hat_dup.astype(np.int64)).max())}")
    print(f"[gen_data] input/dst.bin == v2 golden.bin ; input/t_hat.bin == v2 golden_t_hat.bin")


if __name__ == "__main__":
    main()
