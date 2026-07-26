#!/usr/bin/env python3
"""gen_data — ByteEncode_d 探针：随机 d-bit comp + C ref golden。

在流水线中的位置：本脚本是本探针 golden 的唯一生成入口，产出：
  - input/comp.bin           ：随机压缩系数 int32[256]，供 main.cpp 读入并 launch kernel
  - output/golden_encoded.bin：由 byte_encode_d_ref.c（黑盒 oracle）计算的期望编码结果，
                                供 scripts/verify_result.py 与 kernel 实际输出 output/encoded.bin 对拍
  - input/meta.bin            ：记录 N/d/out_bytes 的元信息（当前 run.sh 未消费，保留供调试/追溯）
与 AscendC 实现的关系：这里的 golden 只保证 I/O 语义（FIPS 203 Alg.5 比特流），
AscendC 侧（byte_encode_d_vec.hpp）实现无需与 _encode_ref 逐行同构。
"""
import os
import struct
import subprocess
import sys

import numpy as np
from ctypes import CDLL, c_int32, c_void_p

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_REF_C = os.path.join(_CASE_DIR, "byte_encode_d_ref.c")
_REF_SO = os.path.join(_CASE_DIR, "scripts", "libbyte_encode_d_ref.so")

N = 256  # 单个多项式的系数个数，与 f203_mlkem_params.h 的 F203_MLKEM_N 保持一致（已锁定参数）。
SEED = 20260628  # 固定随机种子，保证每次 gen_data 生成的输入可复现，便于对拍排错。
# 各 d 值对应的编码输出字节数 = N*d/8，须与 byte_encode_d_config.hpp 的 F203_BYTE_ENCODE_POLY_BYTES 一致。
OUT_BYTES = {4: 128, 5: 160, 10: 320, 11: 352}


def _build_ref_so() -> None:
    """将 byte_encode_d_ref.c 编译为共享库，供下面用 ctypes 加载调用（每次调用前都会重新构建，保证与源码同步）。"""
    os.makedirs(os.path.dirname(_REF_SO), exist_ok=True)
    cmd = [
        "gcc",
        "-shared",
        "-fPIC",
        "-O2",
        "-I",
        _CASE_DIR,
        "-o",
        _REF_SO,
        _REF_C,
    ]
    subprocess.run(cmd, check=True, cwd=_CASE_DIR)


def _encode_ref(comp: np.ndarray, d: int) -> np.ndarray:
    """调用 byte_encode_d_ref.c 中与 d 对应的 poly_byte_encode_d{d}_ref，返回打包后的 golden 比特流。

    参数：
        comp：int32[N] 的压缩系数数组，每元素落在 [0, 2^d)
        d   ：ByteEncode_d 的位宽（4/5/10/11）
    返回：
        uint8[OUT_BYTES[d]] 的打包比特流
    """
    _build_ref_so()
    lib = CDLL(_REF_SO)
    out = np.zeros(OUT_BYTES[d], dtype=np.uint8)
    if d == 4:
        fn = lib.poly_byte_encode_d4_ref
    elif d == 5:
        fn = lib.poly_byte_encode_d5_ref
    elif d == 10:
        fn = lib.poly_byte_encode_d10_ref
    elif d == 11:
        fn = lib.poly_byte_encode_d11_ref
    else:
        raise SystemExit(f"unsupported d={d}")
    fn.argtypes = [c_void_p, c_void_p, c_int32]
    fn.restype = None
    fn(out.ctypes.data_as(c_void_p), comp.ctypes.data_as(c_void_p), N)
    return out


def main() -> None:
    # d 由环境变量 F203_BYTE_ENCODE_D 决定（与 kernel 编译期宏一致，由 run.sh 统一传入）。
    d = int(os.environ.get("F203_BYTE_ENCODE_D", "4"))
    if d not in OUT_BYTES:
        raise SystemExit("F203_BYTE_ENCODE_D must be 4, 5, 10, or 11")

    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    # 用固定种子（SEED+d，避免不同 d 场景生成相同序列）生成 [0, 2^d) 范围内的随机压缩系数。
    rng = np.random.default_rng(SEED + d)
    max_u = (1 << d) - 1
    comp = rng.integers(0, max_u + 1, size=N, dtype=np.int32)
    golden = _encode_ref(comp, d)

    comp.tofile(os.path.join(_CASE_DIR, "input", "comp.bin"))
    golden.tofile(os.path.join(_CASE_DIR, "output", "golden_encoded.bin"))

    # meta.bin：记录 (N, d, out_bytes) 三元组，供人工排错时快速核对本次生成的参数组合。
    meta = struct.pack("<iii", N, d, OUT_BYTES[d])
    with open(os.path.join(_CASE_DIR, "input", "meta.bin"), "wb") as f:
        f.write(meta)

    print(f"[gen_data] N={N} d={d} out_bytes={OUT_BYTES[d]} comp max={comp.max()}")


if __name__ == "__main__":
    main()
