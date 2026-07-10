#!/usr/bin/env python3
"""gen_data — Decompress_d 探针：随机 d-bit comp + C ref golden（d=4/5/10/11）。

本文件在流水线中的位置：run.sh 在编译/运行 kernel 之前调用本脚本，负责：
1) 生成随机的 d-bit 压缩域系数作为设备端输入（input/comp.bin）；
2) 现场编译 decompress_d_ref.c 为共享库并通过 ctypes 调用，产出黑盒 golden（output/golden_poly.bin）。
本脚本本身不是 AscendC 实现规格，仅提供“合法输入 + 期望输出”供 scripts/verify_result.py
与设备端 poly.bin 对拍（I/O 等价校验），对齐 FIPS 203 §4.2.1 Decompress_d。
"""
import os
import struct
import subprocess

import numpy as np
from ctypes import CDLL, c_int32, c_void_p

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_REF_C = os.path.join(_CASE_DIR, "decompress_d_ref.c")
_REF_SO = os.path.join(_CASE_DIR, "scripts", "libdecompress_d_ref.so")

N = 256  # 单个多项式系数个数，对齐 f203_mlkem_params.h 中 F203_MLKEM_N
SEED = 20260628  # 固定随机种子，保证探针每次生成的输入可复现
SUPPORTED_D = (4, 5, 10, 11)  # 本探针验收的解压位宽集合
# d → decompress_d_ref.h 中对应 C 符号名的映射，供 ctypes 按需取函数指针
_REF_FN = {
    4: "poly_decompress_d4_ref",
    5: "poly_decompress_d5_ref",
    10: "poly_decompress_d10_ref",
    11: "poly_decompress_d11_ref",
}


def _build_ref_so() -> None:
    """将 decompress_d_ref.c 现场编译为共享库，供本脚本用 ctypes 加载调用（golden 计算内核）。"""
    os.makedirs(os.path.dirname(_REF_SO), exist_ok=True)
    cmd = ["gcc", "-shared", "-fPIC", "-O2", "-I", _CASE_DIR, "-o", _REF_SO, _REF_C]
    subprocess.run(cmd, check=True, cwd=_CASE_DIR)


def _decompress_ref(comp: np.ndarray, d: int) -> np.ndarray:
    """调用 C 参考实现计算 golden：comp 为长度 N 的 int32 压缩域系数数组，返回同长度解压结果。"""
    _build_ref_so()
    lib = CDLL(_REF_SO)
    out = np.zeros(N, dtype=np.int32)
    fn = getattr(lib, _REF_FN[d])
    fn.argtypes = [c_void_p, c_void_p, c_int32]
    fn.restype = None
    fn(out.ctypes.data_as(c_void_p), comp.ctypes.data_as(c_void_p), N)
    return out


def main() -> None:
    # 解压位宽 d 由环境变量 F203_DECOMPRESS_D 指定，须与 kernel 编译时的同名宏一致，否则 golden 与设备输出语义不匹配。
    d = int(os.environ.get("F203_DECOMPRESS_D", "4"))
    if d not in SUPPORTED_D:
        raise SystemExit(f"F203_DECOMPRESS_D must be one of {SUPPORTED_D}")

    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    # 用固定种子（种子随 d 偏移）生成 [0, 2^d-1] 均匀分布的压缩域系数（合法输入范围由 d 决定）。
    rng = np.random.default_rng(SEED + d)
    max_u = (1 << d) - 1
    comp = rng.integers(0, max_u + 1, size=N, dtype=np.int32)
    golden = _decompress_ref(comp, d)

    # 落盘为设备 kernel 的输入文件与对拍用的 golden 文件（裸 int32 数组，无 header）。
    comp.tofile(os.path.join(_CASE_DIR, "input", "comp.bin"))
    golden.tofile(os.path.join(_CASE_DIR, "output", "golden_poly.bin"))

    # meta.bin 记录 (N, d) 供 run.sh/其它工具在不重新读取环境变量的情况下获知本次数据规模（当前未强制被消费）。
    meta = struct.pack("<ii", N, d)
    with open(os.path.join(_CASE_DIR, "input", "meta.bin"), "wb") as f:
        f.write(meta)

    print(f"[gen_data] N={N} d={d} comp range [0,{max_u}] golden max={golden.max()}")


if __name__ == "__main__":
    main()
