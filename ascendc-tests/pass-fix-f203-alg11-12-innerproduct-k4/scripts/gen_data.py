#!/usr/bin/env python3
# coding=utf-8
"""
@file gen_data.py
@brief 生成内积探针 input/golden：a_hat.bin、s_hat.bin、golden_t_hat.bin。

形状 P_OUT×S_VEC×1（默认 4×4），N=256；GM 与 alg13 a_hat 行主序一致。
golden 由编译 hat_inner_product_ref.c 得到的 C 参考计算（仅 I/O oracle）。
"""
import ctypes
import os
import subprocess

import numpy as np

from ip_shape import N, P_OUT, S_VEC

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))

Q = 3329
SEED = 20260617


def make_a_hat() -> np.ndarray:
    """构造随机 Â：K×K 行主序，flat(p,j,c)=(p*S_VEC+j)*N+c，系数 ∈ [0,Q)。"""
    rng = np.random.default_rng(SEED)
    return rng.integers(0, Q, size=(P_OUT * S_VEC, N), dtype=np.int32)


def make_s_hat() -> np.ndarray:
    """构造随机 ŝ：[S_VEC, N]，系数 ∈ [0,Q)。"""
    rng = np.random.default_rng(SEED + 1)
    return rng.integers(0, Q, size=(S_VEC, N), dtype=np.int32)


def build_c_ref():
    """
    编译 hat_inner_product_ref.c 为临时 .so，返回 (hat_inner_product_dot, so_path)。
    宏 HAT_P_OUT / HAT_S_VEC 与本脚本形状一致。
    """
    src = os.path.join(_CASE_DIR, "hat_inner_product_ref.c")
    out = os.path.join(_SCRIPT_DIR, "_hat_ip_ref.so")
    subprocess.check_call(
        [
            "gcc",
            "-shared",
            "-fPIC",
            "-O2",
            "-o",
            out,
            src,
            f"-I{_CASE_DIR}",
            f"-DHAT_P_OUT={P_OUT}",
            f"-DHAT_S_VEC={S_VEC}",
        ]
    )
    lib = ctypes.CDLL(out)
    fn = lib.hat_inner_product_dot
    fn.argtypes = [
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.c_int,
    ]
    fn.restype = None
    return fn, out


def main() -> None:
    """写 input/*.bin 与 output/golden_t_hat.bin；mod_variant=0（SCALAR_I64）。"""
    if P_OUT != S_VEC:
        print(f"[WARN] gen_data: P_OUT={P_OUT} S_VEC={S_VEC} (KeyGen K=4 通常相等)")
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    a_hat = make_a_hat()
    s_hat = make_s_hat()

    fn, so_path = build_c_ref()
    t_hat = np.zeros((P_OUT, N), dtype=np.int32)
    # mod_variant=0 → HAT_MOD_SCALAR_I64
    fn(
        a_hat.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        s_hat.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        t_hat.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        0,
    )
    os.remove(so_path)

    a_hat.tofile(os.path.join(_CASE_DIR, "input", "a_hat.bin"))
    s_hat.tofile(os.path.join(_CASE_DIR, "input", "s_hat.bin"))
    t_hat.tofile(os.path.join(_CASE_DIR, "output", "golden_t_hat.bin"))

    print(f"shape={P_OUT}x{S_VEC}x1 N={N} layout=a_hat row-major (p*K+j)*N")
    print(f"golden=hat_inner_product_dot (no e_hat)")
    print(f"t_hat[0,:6] = {t_hat[0, :6].tolist()}")
    print("[OK] gen_data: a_hat.bin s_hat.bin golden_t_hat.bin")


if __name__ == "__main__":
    main()
