#!/usr/bin/env python3
# coding=utf-8
"""Golden for halfrows inner product; GM layout = alg13 a_hat row-major."""
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
    rng = np.random.default_rng(SEED)
    return rng.integers(0, Q, size=(P_OUT * S_VEC, N), dtype=np.int32)


def make_s_hat() -> np.ndarray:
    rng = np.random.default_rng(SEED + 1)
    return rng.integers(0, Q, size=(S_VEC, N), dtype=np.int32)


def build_c_ref():
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
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    a_hat = make_a_hat()
    s_hat = make_s_hat()

    fn, so_path = build_c_ref()
    t_hat = np.zeros((P_OUT, N), dtype=np.int32)
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

    print(f"shape=4x4x1 N={N} layout=a_hat row-major (p*K+j)*N")
    print(f"t_hat[0,:6] = {t_hat[0, :6].tolist()}")
    print("[OK] gen_data: a_hat.bin s_hat.bin golden_t_hat.bin")


if __name__ == "__main__":
    main()
