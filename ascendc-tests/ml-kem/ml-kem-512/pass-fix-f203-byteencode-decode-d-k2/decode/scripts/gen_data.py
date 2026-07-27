#!/usr/bin/env python3
"""gen_data — ByteDecode_d：round-trip 经 byteencode ref 生成 encoded + golden comp。

在流水线中的位置：本脚本是本探针 golden 的唯一生成入口。因为 ByteDecode_d 的输入
（打包比特流）不便直接随机构造出合法值，故先随机生成压缩系数 comp，再借用同树
`../encode/`（ByteEncode_d 子探针）的 byte_encode_d_ref.c 把它编码为 encoded，
最后用本目录 byte_decode_d_ref.c 解码回 golden 并与原始 comp 做 round-trip 自检，
确保生成的 (encoded, golden_comp) 对本身就是自洽的合法样本。产出：
  - input/encoded.bin     ：encode ref 生成的打包比特流，供 main.cpp 读入并 launch kernel
  - output/golden_comp.bin：decode ref 还原的系数（== 原始 comp），供 verify_result.py 对拍
  - input/meta.bin         ：记录 N/d/in_bytes 的元信息（当前 run.sh 未消费，保留供调试/追溯）
与 AscendC 实现的关系：这里的 golden 只保证 I/O 语义（FIPS 203 Alg.6 语义），
AscendC 侧（byte_decode_d_vec.hpp）实现无需与 _decode_ref 逐行同构。
"""
import os
import struct
import subprocess
import sys

import numpy as np
from ctypes import CDLL, c_int32, c_void_p

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
# 同树 encode/ 子探针：复用 byte_encode_d_ref.c 生成合法 encoded（512 W0/B2 布局）。
_ENCODE_DIR = os.path.normpath(os.path.join(_CASE_DIR, "..", "encode"))
_DECODE_REF_C = os.path.join(_CASE_DIR, "byte_decode_d_ref.c")
_ENCODE_REF_C = os.path.join(_ENCODE_DIR, "byte_encode_d_ref.c")
_REF_SO = os.path.join(_CASE_DIR, "scripts", "libbytedecode_gen.so")

N = 256  # 单个多项式的系数个数，与 f203_mlkem_params.h 的 F203_MLKEM_N 保持一致（已锁定参数）。
SEED = 20260628  # 固定随机种子，保证每次 gen_data 生成的输入可复现，便于对拍排错。
# 各 d 值对应的解码输入字节数 = N*d/8，须与 byte_decode_d_config.hpp 的 F203_BYTE_DECODE_POLY_BYTES 一致。
IN_BYTES = {4: 128, 5: 160, 10: 320, 11: 352}


def _build_ref_so() -> None:
    """把本目录 byte_decode_d_ref.c 与上游 byte_encode_d_ref.c 一起编译为共享库，
    供下面 _encode_ref/_decode_ref 用 ctypes 加载调用。"""
    os.makedirs(os.path.dirname(_REF_SO), exist_ok=True)
    cmd = [
        "gcc",
        "-shared",
        "-fPIC",
        "-O2",
        "-I",
        _CASE_DIR,
        "-I",
        _ENCODE_DIR,
        "-o",
        _REF_SO,
        _DECODE_REF_C,
        _ENCODE_REF_C,
    ]
    subprocess.run(cmd, check=True, cwd=_CASE_DIR)


def _encode_ref(comp: np.ndarray, d: int) -> np.ndarray:
    """调用上游 poly_byte_encode_d{d}_ref，把随机压缩系数 comp 编码为合法的打包比特流。"""
    _build_ref_so()
    lib = CDLL(_REF_SO)
    out = np.zeros(IN_BYTES[d], dtype=np.uint8)
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


def _decode_ref(encoded: np.ndarray, d: int) -> np.ndarray:
    """调用本目录 poly_byte_decode_d{d}_ref，把打包比特流还原为系数（golden）。"""
    _build_ref_so()
    lib = CDLL(_REF_SO)
    out = np.zeros(N, dtype=np.int32)
    if d == 4:
        fn = lib.poly_byte_decode_d4_ref
    elif d == 5:
        fn = lib.poly_byte_decode_d5_ref
    elif d == 10:
        fn = lib.poly_byte_decode_d10_ref
    elif d == 11:
        fn = lib.poly_byte_decode_d11_ref
    else:
        raise SystemExit(f"unsupported d={d}")
    fn.argtypes = [c_void_p, c_void_p, c_int32]
    fn.restype = None
    fn(out.ctypes.data_as(c_void_p), encoded.ctypes.data_as(c_void_p), N)
    return out


def main() -> None:
    # d 由环境变量 F203_BYTE_DECODE_D 决定（与 kernel 编译期宏一致，由 run.sh 统一传入）。
    d = int(os.environ.get("F203_BYTE_DECODE_D", "4"))
    if d not in IN_BYTES:
        raise SystemExit("F203_BYTE_DECODE_D must be 4, 5, 10, or 11")

    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    # 用固定种子（SEED+d，避免不同 d 场景生成相同序列）生成 [0, 2^d) 范围内的随机压缩系数，
    # 再走 encode→decode round-trip：encoded 作为 kernel 输入，golden(=还原后的 comp) 作为期望输出。
    rng = np.random.default_rng(SEED + d)
    max_u = (1 << d) - 1
    comp = rng.integers(0, max_u + 1, size=N, dtype=np.int32)
    encoded = _encode_ref(comp, d)
    golden = _decode_ref(encoded, d)

    # 自检：若 encode/decode ref 本身不能还原出原始 comp，说明参考实现有 bug，直接中止生成。
    if not np.array_equal(golden, comp):
        raise SystemExit("[gen_data] encode/decode ref round-trip failed")

    encoded.tofile(os.path.join(_CASE_DIR, "input", "encoded.bin"))
    comp.tofile(os.path.join(_CASE_DIR, "output", "golden_comp.bin"))

    # meta.bin：记录 (N, d, in_bytes) 三元组，供人工排错时快速核对本次生成的参数组合。
    meta = struct.pack("<iii", N, d, IN_BYTES[d])
    with open(os.path.join(_CASE_DIR, "input", "meta.bin"), "wb") as f:
        f.write(meta)

    print(f"[gen_data] N={N} d={d} in_bytes={IN_BYTES[d]} round-trip OK")


if __name__ == "__main__":
    main()
