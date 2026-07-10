#!/usr/bin/python3
# F203 交付语义参考（sepolyvec8_ntt_f203）：Stage1 hi/lo + 右 LUT MatMul + RouteA Stage3 + mod。
# 流水线位置：verify_stage / 调试用 Host 参考；仅提供 golden 语义，非 AscendC 实现规格。
import re
from pathlib import Path

Q = 3329
N = 256
K = 8
M_MAT_A = 2 * K
OUT_COLS = 512

_REPO = Path(__file__).resolve().parents[1]  # scripts/ → 探针根
_TABLES_H = _REPO / "thirdparty/ntt_study/include/mlkem/stable/mlkem_ntt_tables.h"
_F203_CASE = _REPO / "thirdparty/ntt_study/deliverables/sepolyvec8_ntt_f203"


def _parse_i16_array(text: str, symbol: str, expect: int) -> list:
    m = re.search(rf"{symbol}\s*\[[^\]]*\]\s*=\s*\{{(.*?)\}};", text, re.S)
    if not m:
        raise ValueError(f"Cannot locate {symbol} in {_TABLES_H}")
    nums = [int(x) for x in re.findall(r"-?\d+", m.group(1))]
    if len(nums) != expect:
        raise ValueError(f"{symbol} size mismatch: got {len(nums)}, expect {expect}")
    return nums


def load_zetas() -> list:
    """从 mlkem_ntt_tables.h 解析 kMlkemZetas。"""
    text = _TABLES_H.read_text(encoding="utf-8")
    return _parse_i16_array(text, "kMlkemZetas", 128)


def gen_fixed_se_polyvec() -> "np.ndarray":
    """生成固定可复现 se[8,256]（调试用，非 gen_data 随机路径）。"""
    import numpy as np

    a = np.zeros((K, N), dtype=np.int32)
    for poly in range(K):
        for j in range(N):
            if poly < K // 2:
                a[poly, j] = ((poly + 1) * 257 + j * 17 + (j % 13) * 19) % Q
            else:
                pe = poly - K // 2
                a[poly, j] = ((pe + 1) * 911 + j * 29 + (j % 11) * 23 + 7) % Q
    return a


def load_f203_se_and_lut_fp16():
    import numpy as np

    se = np.fromfile(_F203_CASE / "input0.bin", dtype=np.int32).reshape(K, N)
    lut = np.fromfile(_F203_CASE / "input1.bin", dtype=np.float16).reshape(N, OUT_COLS)
    return se, lut


def fp16_lut_to_i8(lut_fp16: "np.ndarray") -> "np.ndarray":
    import numpy as np

    return np.clip(np.rint(lut_fp16.astype(np.float32)), -128, 127).astype(np.int8)


def load_mat_b_lut_i8() -> "np.ndarray":
    _, lut_fp16 = load_f203_se_and_lut_fp16()
    return fp16_lut_to_i8(lut_fp16)


def validate_se_polyvec_in_zq(se: "np.ndarray") -> None:
    """Stage1 输入契约：s||e 系数已在 Z_q 且非负，Stage1 不做 mod q。

    与交付 sepolyvec8_ntt_f203 一致：图内仅 hi/lo 拆分，无 ReduceToZq。
    在 [0, Q) 下 hi = v>>6 ∈ [0,51]、lo = v&63 ∈ [0,63]，均落在 int8 内。
    """
    import numpy as np

    if se.shape != (K, N):
        raise ValueError(f"se shape mismatch: got {se.shape}, expect ({K}, {N})")
    if se.dtype != np.int32:
        raise ValueError(f"se dtype must be int32, got {se.dtype}")
    vmin = int(se.min())
    vmax = int(se.max())
    if vmin < 0 or vmax >= Q:
        raise ValueError(f"se out of [0,{Q}): min={vmin}, max={vmax}")


def encode_mat_a(se: "np.ndarray") -> "np.ndarray":
    """F203 Stage1：双 polyvec [HI(8), LO(8)] -> [16,256] int8。

    语义对齐 MlkemEncodeToLimb6 / 交付 ONNX（Floor 路径在 v>=0 时等价于位运算）：
      hi = v >> 6, lo = v & 63
    """
    import numpy as np

    validate_se_polyvec_in_zq(se)
    hi_i8 = (se >> 6).astype(np.int8)
    lo_i8 = (se & 63).astype(np.int8)
    return np.concatenate([hi_i8, lo_i8], axis=0)


def matmul_int8_i32(a: "np.ndarray", b: "np.ndarray") -> "np.ndarray":
    import numpy as np

    return np.matmul(a.astype(np.int32), b.astype(np.int32)).astype(np.int32)


def f203_stage3_route_a(mat_c: "np.ndarray") -> "np.ndarray":
    """RouteA：mat_c [16,512] -> raw [8,256]，对齐 sepolyvec8_ntt_f203 ONNX。"""
    import numpy as np

    hi_rows = mat_c[0:K, :]
    lo_rows = mat_c[K : 2 * K, :]
    hh = hi_rows[:, 0::2]
    hl = lo_rows[:, 0::2]
    lh = hi_rows[:, 1::2]
    ll = lo_rows[:, 1::2]
    raw = (
        hh.astype(np.int64) * 4096
        + (hl.astype(np.int64) + lh.astype(np.int64)) * 64
        + ll.astype(np.int64)
    )
    return raw.astype(np.int32)


def stage31_mod(raw: "np.ndarray") -> "np.ndarray":
    """mod q；保留 ONNX/ntt_study 双校正写法（CANN 9.0.0 下对合法输入为恒等）。"""
    import numpy as np

    raw64 = raw.astype(np.int64)
    q = np.int64(Q)
    t = np.where(raw64 >= 0, raw64 // q, -((-raw64) // q))
    rem = raw64 - q * t
    # ntt_study 时代 Div 底层问题兜底；数学上 floor 除法后 rem 已在 [0,q)
    rem = rem - q * (rem >= q).astype(np.int64)
    rem = rem + q * (rem < 0).astype(np.int64)
    return rem.astype(np.int32)


def f203_three_stage_batch(se: "np.ndarray", mat_b: "np.ndarray | None" = None) -> "np.ndarray":
    """完整 F203 int8 Cube 路径 -> [8,256] int32。"""
    if mat_b is None:
        mat_b = load_mat_b_lut_i8()
    mat_a = encode_mat_a(se)
    mat_c = matmul_int8_i32(mat_a, mat_b)
    raw = f203_stage3_route_a(mat_c)
    return stage31_mod(raw)


def mlkem_reduce_to_zq(x: int) -> int:
    x = int(x)
    if x < 0:
        x += Q
    q1 = (x * 78) >> 18
    x -= q1 * Q
    q2 = (x * 5039) >> 24
    x -= q2 * Q
    if x >= Q:
        x -= Q
    return x


def mlkem_ntt(coeffs: list) -> list:
    zetas = load_zetas()
    f = [mlkem_reduce_to_zq(c) for c in coeffs]
    i = 1
    for length in (128, 64, 32, 16, 8, 4, 2):
        for start in range(0, N, 2 * length):
            zeta = zetas[i]
            i += 1
            for j in range(start, start + length):
                t = mlkem_reduce_to_zq(zeta * f[j + length])
                f[j + length] = mlkem_reduce_to_zq(f[j] - t)
                f[j] = mlkem_reduce_to_zq(f[j] + t)
    return f


def golden_mlkem_ntt_batch(se: "np.ndarray") -> "np.ndarray":
    import numpy as np

    out = np.zeros((K, N), dtype=np.int32)
    for poly in range(K):
        out[poly] = np.array(mlkem_ntt(se[poly].tolist()), dtype=np.int32)
    return out
