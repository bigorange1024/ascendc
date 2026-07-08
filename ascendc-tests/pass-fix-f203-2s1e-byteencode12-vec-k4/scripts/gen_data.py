#!/usr/bin/env python3
# coding=utf-8
"""
gen_data.py — ByteEncode₁₂-only；**输入契约与 vec-k4-v2 Alg.13 mixPass=7 完全一致**。

与集成路径对齐（可直接替换 2s1e_post_ntt_ub stageEncodeOut 的 GM preset）：
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
    if not os.path.isfile(_V2_GEN):
        raise SystemExit(f"[gen_data] missing v2 gen_data: {_V2_GEN}")
    spec = importlib.util.spec_from_file_location("v2_gen_data", _V2_GEN)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main() -> None:
    v2 = _load_v2_gen()
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

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
    ek_ref = v2.golden_byte_encode_polyvec(t_hat_ref)
    sk_ref = v2.golden_byte_encode_polyvec(s_hat)

    # tiling 不再由 Python 落盘：运行时改由 byte_encode12_tiling.cpp 的 GenerateTiling 生成。

    dst_ref.astype(np.int32).tofile(os.path.join(_CASE_DIR, "input", "dst.bin"))
    t_hat_ref.astype(np.int32).tofile(os.path.join(_CASE_DIR, "input", "t_hat.bin"))

    ek_ref.tofile(os.path.join(_CASE_DIR, "output", "golden_ek_polyvec.bin"))
    sk_ref.tofile(os.path.join(_CASE_DIR, "output", "golden_sk_polyvec.bin"))

    dst_ref.tofile(os.path.join(_CASE_DIR, "output", "golden_dst.bin"))
    t_hat_ref.tofile(os.path.join(_CASE_DIR, "output", "golden_t_hat.bin"))

    s_hat_dup = dst_ref[K_S : 2 * K_S]
    print(f"[gen_data] Alg.13 contract: dst {dst_ref.shape} t_hat {t_hat_ref.shape} ek/sk {POLYVEC_BYTES}B each")
    print(f"[gen_data] s_hat dup max_abs_diff={int(np.abs(s_hat.astype(np.int64) - s_hat_dup.astype(np.int64)).max())}")
    print(f"[gen_data] input/dst.bin == v2 golden.bin ; input/t_hat.bin == v2 golden_t_hat.bin")


if __name__ == "__main__":
    main()
