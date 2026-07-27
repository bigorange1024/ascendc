#!/usr/bin/env python3
"""
@file gen_data.py
@brief 4-poly 紧凑三段式 NTT/INTT 的 golden / 输入生成（与设备 Tag5T 同构）。

流水线位置：run.sh 在编译/跑核前调用；写出 input/{src,lut_*,tiling}.bin 与
output/{golden_dst,golden_s0,golden_mat_c}.bin。

作用（黑盒 oracle，非 AscendC 规格）：
  1. 按 F203_NTT_MODE 从 transpose_mlkem_luts_i8.h 解析 NTT 或 INTT LUT；
  2. 随机 src [4,256] → 紧凑 S0 [8,256] → 四路 matmul → 平面 mat_c [32,128] → RouteA+stage31_mod → dst；
  3. 可选与 ntt_study deliverable 交叉核对（仅 NTT 且 src 碰巧一致时）。

与 golden 关系：verify_result.py 只比 dst vs golden_dst；s0/mat_c 供分段调试。
环境变量：F203_NTT_MODE=ntt|intt；STAGE123_POLYVEC4_MIX_PASS（写入 tiling.bin）。
"""
import os
import re
import struct
import subprocess
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_NTT_LUT_HDR = os.path.normpath(
    os.path.join(_CASE_DIR, "thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h")
)
_MLKEM_REF = _SCRIPT_DIR
_NTT_LUT_HDR = os.path.normpath(
    os.path.join(_CASE_DIR, "thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h")
)
_NTT_STUDY_GOLDEN = os.path.normpath(
    os.path.join(_CASE_DIR, "thirdparty/ntt_onnx/deliverables/sepolyvec4_ntt_f203")
)
sys.path.insert(0, _MLKEM_REF)
from mlkem_ref import stage31_mod  # noqa: E402

# ---- 与 tiling.h / 设备布局对齐的常量（禁止擅自改形状）----
N = 256
HALF_N = N // 2
K = 4
K_PER_AIV = 2          # poly-batch：每 AIV 2 条，连续 {0,1}|{2,3}
M_ROWS = 2 * K         # 紧凑 S0：HI₄+LO₄
LIMBS = 4
MAT_C_PLANAR_ROWS = K * LIMBS * 2  # 32
LIMB_MASK = 0x3F
LIMB_BITS = 6
Q = 3329
SEED = 20260628


def load_lut_t_i8(mode: str) -> np.ndarray:
    """
    从 LUT 头文件解析 kMlkemLimb6Ntt_T_i8 或 Intt 符号。

    输入：mode 'ntt'|'intt'
    输出：int8 数组形状 [256, 512]（行=系数维，列=even/odd 拼 512）
    前置：头文件存在且数组字面量可被正则扫出 expect=N*512 个数
    """
    symbol = "kMlkemLimb6Ntt_T_i8" if mode == "ntt" else "kMlkemLimb6Intt_T_i8"
    with open(_NTT_LUT_HDR, encoding="utf-8") as f:
        txt = f.read()
    i0 = txt.index(symbol)
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    body = txt[i1 + 1 : i2]
    nums = [int(x) for x in re.findall(r"-?\d+", body)]
    expect = N * 512
    if len(nums) != expect:
        raise SystemExit(f"[gen_data] LUT {symbol} size {len(nums)} != {expect}")
    return np.array(nums, dtype=np.int8).reshape(N, 512)


def lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    """
    将 [256,512] LUT 拆成设备用的 stacked 平面 [512,128]。

    even=True：偶列 top=[:,0:N:2]、bottom=[:,N:512:2]；
    even=False：奇列同理。concatenate 后行 0..255=top，256..511=bottom，
    对应 tiling LUT_EVEN/ODD_TOP/BOTTOM。
    """
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def planar_row(slot: int, limb: int, half: int) -> int:
    """平面 mat_c 行号，与 aiv_func planar_k4::mat_row 同构。"""
    return half * (K * LIMBS) + slot * LIMBS + limb


def encode_compact_k4(batch: np.ndarray, s0: np.ndarray) -> None:
    """
    Stage1 紧凑编码：batch[k,N] → s0 的 HI 行 [0..k) 与 LO 行 [K..K+k)。

    对每个系数 v：先 %Q，再 hi=(v>>6)&63 写入 s0[lp]，lo=v&63 写入 s0[K+lp]。
    就地写入 s0；与设备 lo=v-hi*64 在 v∈[0,Q) 时等价。
    """
    k = batch.shape[0]
    for lp in range(k):
        for r in range(N):
            v = int(batch[lp, r]) % Q
            s0[lp, r] = (v >> LIMB_BITS) & LIMB_MASK
            s0[K + lp, r] = v & LIMB_MASK


def encode_k4_s0(polys: np.ndarray) -> np.ndarray:
    """分配零 S0 [8,256] int8 并 encode_compact_k4 全 4 poly。"""
    s0 = np.zeros((M_ROWS, N), dtype=np.int8)
    encode_compact_k4(polys, s0)
    return s0


def mat_c_tmp_golden(s0: np.ndarray, lut: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """
    Stage2 四路 matmul golden：s0[8,256] @ LUT 四半块 → 各 [8,128] int32。

    le/lo/he/ho 对应设备 LUT_EVEN_TOP / ODD_TOP / EVEN_BOTTOM / ODD_BOTTOM。
    """
    le = lut[:, 0:N:2]
    lo = lut[:, 1:N:2]
    he = lut[:, N:512:2]
    ho = lut[:, N + 1 : 512 : 2]
    c_lo_even = (s0.astype(np.int32) @ le.astype(np.int32)).astype(np.int32)
    c_lo_odd = (s0.astype(np.int32) @ lo.astype(np.int32)).astype(np.int32)
    c_hi_even = (s0.astype(np.int32) @ he.astype(np.int32)).astype(np.int32)
    c_hi_odd = (s0.astype(np.int32) @ ho.astype(np.int32)).astype(np.int32)
    return c_lo_even, c_lo_odd, c_hi_even, c_hi_odd


def pack_bank_planar_k4(
    c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, poly_base: int, k_polys: int, out: np.ndarray
) -> None:
    """
    将一段 poly-batch（poly_base 起 k_polys 条）的四临时写入平面 out。

    与 AivK4PackMatCPlanar 同构：half=0 用 c_lo_*，half=1 用 c_hi_*；
    每 slot 四 limb 行来自 HI/LO 行 × even/odd。
    """
    for lp in range(k_polys):
        hi_r = poly_base + lp
        lo_r = K + poly_base + lp
        slot = poly_base + lp
        out[planar_row(slot, 0, 0), :] = c_lo_even[hi_r, :]
        out[planar_row(slot, 1, 0), :] = c_lo_odd[hi_r, :]
        out[planar_row(slot, 2, 0), :] = c_lo_even[lo_r, :]
        out[planar_row(slot, 3, 0), :] = c_lo_odd[lo_r, :]
        out[planar_row(slot, 0, 1), :] = c_hi_even[hi_r, :]
        out[planar_row(slot, 1, 1), :] = c_hi_odd[hi_r, :]
        out[planar_row(slot, 2, 1), :] = c_hi_even[lo_r, :]
        out[planar_row(slot, 3, 1), :] = c_hi_odd[lo_r, :]


def pack_mat_c_planar_k4(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd) -> np.ndarray:
    """全 4 poly：先 AIV0 的 0..1，再 AIV1 的 2..3，得到 [32,128] 平面。"""
    out = np.zeros((MAT_C_PLANAR_ROWS, HALF_N), dtype=np.int32)
    pack_bank_planar_k4(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, 0, K_PER_AIV, out)
    pack_bank_planar_k4(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, K_PER_AIV, K_PER_AIV, out)
    return out


def merge_planar_poly(mat_planar: np.ndarray, slot: int) -> np.ndarray:
    """
    Stage3 RouteA：对单个 slot 读八行（两 half × 四 limb），Horner 成 raw 再 stage31_mod。

    raw = hh*4096 + (hl+lh)*64 + ll（与设备两次 <<6 等价）；
    输出 [256]：前半 half=0，后半 half=1。
    """
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


def golden_dst_from_planar(mat_planar: np.ndarray) -> np.ndarray:
    """对 slot 0..3 调用 merge_planar_poly，得到 golden dst [4,256]。"""
    dst = np.zeros((K, N), dtype=np.int32)
    for slot in range(K):
        dst[slot] = merge_planar_poly(mat_planar, slot)
    return dst


def try_compare_ntt_study(src: np.ndarray, golden: np.ndarray, mode: str) -> None:
    """
    可选：若 ntt_study deliverable 的 input0 与当前 src 逐元素相同，则打印与其 golden 的 max diff。
    仅 mode=='ntt'；缺 bin 则跳过。不改变写出的本地 golden。
    """
    if mode != "ntt":
        return
    ref_path = os.path.join(_NTT_STUDY_GOLDEN, "golden.bin")
    in_path = os.path.join(_NTT_STUDY_GOLDEN, "input0.bin")
    if not (os.path.isfile(ref_path) and os.path.isfile(in_path)):
        print("[gen_data] ntt_study deliverable bins missing; skip cross-check")
        return
    ref_in = np.fromfile(in_path, dtype=np.int32).reshape(K, N)
    ref_golden = np.fromfile(ref_path, dtype=np.int32).reshape(K, N)
    if np.array_equal(ref_in, src):
        diff = int(np.max(np.abs(golden.astype(np.int64) - ref_golden.astype(np.int64))))
        print(f"[gen_data] ntt_study deliverable cross-check max={diff}")
    else:
        print("[gen_data] src differs from ntt_study input0.bin; using local golden only")


def main() -> None:
    """
    生成全套 input/output bin。

    流程：读环境 → 随机 src → LUT → S0 → 四临时 → 平面 → dst → 落盘 tiling。
    """
    mode = os.environ.get("F203_NTT_MODE", "ntt").lower()
    if mode not in ("ntt", "intt"):
        raise SystemExit("F203_NTT_MODE must be ntt or intt")
    mix_pass = int(os.environ.get("STAGE123_POLYVEC4_MIX_PASS", "3"))

    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    # INTT 用不同种子，避免与 NTT 输入撞车
    rng = np.random.default_rng(SEED + (0 if mode == "ntt" else 1))
    src = rng.integers(0, Q, size=(K, N), dtype=np.int32)

    lut = load_lut_t_i8(mode)
    lut_even = lut_planar_stacked(lut, True)
    lut_odd = lut_planar_stacked(lut, False)

    s0 = encode_k4_s0(src)
    c_le, c_lo, c_he, c_ho = mat_c_tmp_golden(s0, lut)
    mat_planar = pack_mat_c_planar_k4(c_le, c_lo, c_he, c_ho)
    golden_dst = golden_dst_from_planar(mat_planar)

    src.tofile(os.path.join(_CASE_DIR, "input", "src.bin"))
    lut_even.tofile(os.path.join(_CASE_DIR, "input", "lut_even_stacked.bin"))
    lut_odd.tofile(os.path.join(_CASE_DIR, "input", "lut_odd_stacked.bin"))
    golden_dst.tofile(os.path.join(_CASE_DIR, "output", "golden_dst.bin"))
    s0.tofile(os.path.join(_CASE_DIR, "output", "golden_s0.bin"))
    mat_planar.tofile(os.path.join(_CASE_DIR, "output", "golden_mat_c.bin"))

    # tiling.bin：little-endian int32 ×3（N,K,mixPass），垫到 64 字节
    tiling = struct.pack("<iii", N, K, mix_pass)
    with open(os.path.join(_CASE_DIR, "input", "tiling.bin"), "wb") as f:
        f.write(tiling.ljust(64, b"\x00"))

    try_compare_ntt_study(src, golden_dst, mode)
    print(f"[gen_data] K={K} mode={mode} mixPass={mix_pass}")


if __name__ == "__main__":
    main()
