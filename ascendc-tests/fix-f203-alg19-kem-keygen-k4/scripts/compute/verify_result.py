#!/usr/bin/env python3
# @probe stable-mlkem-f203-pke-keygen-k4
# @file scripts/compute/verify_result.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `verify_result.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps. compute 子树可单独跑中间 bin（调试）。 / Compute subtree debug bins optional.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify python3 调用或由 run.sh 对拍 output vs golden。

# coding=utf-8
"""
verify_result.py — 2s1e vec-k4-v2 对拍脚本。

## 检查矩阵（mixPass=0 一次 launch 全比）

| 输出 | golden | 阶段 |
|------|--------|------|
| s0.bin | golden_s0.bin | Stage1 |
| mat_c.bin | golden_mat_c.bin | Stage2 平面 |
| dst.bin | golden.bin | Stage3；另比 s_hat 双块一致 |
| t_hat.bin | golden_t_hat_dot（DOT_ONLY）或 golden_t_hat | 行 18 |
| ek/sk_polyvec | golden_ek/sk | 行 19–20（HAT_BYTE_ENCODE=1） |
| ek_pke.bin | golden_ek_pke | 行 21 ek‖ρ（F203_KEYGEN_EK_PKE=1） |

## CPU/SIM mixPass=0

  单次 launch：S1→S2 MMAD→S3→行18–21 全在 AscendC；verify 比对 s0/mat_c/dst。

## 环境变量

  HAT_LINE18_DOT_ONLY=1 → 比 golden_t_hat_dot（无 ê）
  HAT_BYTE_ENCODE=1       → 比 ek/sk
  F203_KEYGEN_EK_PKE=1    → 比 ek_pke（行 21 内核融合，无额外 launch）
"""
import os
import sys

import numpy as np

N = 256
K_S = 4
K_E = 4
K_HAT = 4
K_DST = 2 * K_S + K_E
M_ROWS = 32
HALF_N = N // 2
MAT_C_PLANAR_ROWS = 96
POLYVEC_BYTES = K_HAT * 384
RHO_BYTES = 32
EK_PKE_BYTES = POLYVEC_BYTES + RHO_BYTES


def check(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    diff = np.abs(got.astype(np.int64) - ref.astype(np.int64))
    mx = int(diff.max())
    if mx != 0:
        flat = int(diff.argmax())
        print(f"[FAIL] {label} max_abs_diff={mx} flat_idx={flat}")
        print(f"  expected={int(ref.reshape(-1)[flat])} actual={int(got.reshape(-1)[flat])}")
        return 1
    print(f"[SUCCESS] {label} max_abs_diff=0")
    return 0


def check_bytes(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    if got.shape != ref.shape:
        print(f"[FAIL] {label} shape {got.shape} vs {ref.shape}")
        return 1
    diff = np.abs(got.astype(np.int16) - ref.astype(np.int16))
    mx = int(diff.max())
    if mx != 0:
        flat = int(diff.argmax())
        print(f"[FAIL] {label} max_abs_diff={mx} byte_idx={flat}")
        return 1
    print(f"[SUCCESS] {label} max_abs_diff=0")
    return 0


def _read_mix_pass() -> int | None:
    path = "./input/tiling.bin"
    if not os.path.isfile(path):
        return None
    t = np.fromfile(path, dtype=np.int32, count=16)
    return int(t[2]) if t.size >= 3 else None


def main() -> int:
    """逐项 check；HAT_LINE18_DOT_ONLY / HAT_BYTE_ENCODE 决定比哪些 golden。"""
    rc = 0
    mix_pass = _read_mix_pass()
    skip_ntt = mix_pass in (4, 7)
    if skip_ntt:
        print(f"[verify] mixPass={mix_pass} — skip S0/mat_c (preset path)")
    if not skip_ntt and os.path.isfile("./output/golden_s0.bin"):
        s_path = "./output/s0.bin"
        if os.path.isfile("./output/_checkpoint_s0.bin"):
            s_path = "./output/_checkpoint_s0.bin"
        if os.path.isfile(s_path):
            g = np.fromfile("./output/golden_s0.bin", dtype=np.int8).reshape(M_ROWS, N)
            s = np.fromfile(s_path, dtype=np.int8).reshape(M_ROWS, N)
            rc |= check("S0 2s1e vs golden_s0", s, g)

    if not skip_ntt and os.path.isfile("./output/golden_mat_c.bin"):
        c_path = "./output/mat_c.bin"
        if os.path.isfile("./output/_checkpoint_mat_c.bin"):
            c_path = "./output/_checkpoint_mat_c.bin"
        if os.path.isfile(c_path):
            g = np.fromfile("./output/golden_mat_c.bin", dtype=np.int32).reshape(MAT_C_PLANAR_ROWS, HALF_N)
            c = np.fromfile(c_path, dtype=np.int32).reshape(MAT_C_PLANAR_ROWS, HALF_N)
            rc |= check("mat_c_planar vs golden", c, g)

    if os.path.isfile("./output/golden.bin") and os.path.isfile("./output/dst.bin"):
        g = np.fromfile("./output/golden.bin", dtype=np.int32).reshape(K_DST, N)
        d = np.fromfile("./output/dst.bin", dtype=np.int32).reshape(K_DST, N)
        rc |= check("dst vs golden (12-poly)", d, g)
        s0 = d[:K_S]
        s1 = d[K_S : 2 * K_S]
        rc |= check("s_hat_aiv0 vs s_hat_aiv1 (device)", s0, s1)
        s_hat_ref = g[:K_S]
        rc |= check("s_hat_aiv0 vs golden s_hat", s0, s_hat_ref)
        rc |= check("s_hat_aiv1 vs golden s_hat", s1, s_hat_ref)
        e_ref = g[2 * K_S :]
        e_got = d[2 * K_S :]
        rc |= check("e_hat vs golden e_hat", e_got, e_ref)

    dot_only = os.environ.get("HAT_LINE18_DOT_ONLY", "1") == "1"

    if dot_only and os.path.isfile("./output/golden_t_hat_dot.bin") and os.path.isfile("./output/t_hat.bin"):
        g = np.fromfile("./output/golden_t_hat_dot.bin", dtype=np.int32).reshape(K_HAT, N)
        t = np.fromfile("./output/t_hat.bin", dtype=np.int32).reshape(K_HAT, N)
        rc |= check("t_hat dot vs golden (Â·ŝ, no ê)", t, g)
    elif os.path.isfile("./output/golden_t_hat.bin") and os.path.isfile("./output/t_hat.bin"):
        g = np.fromfile("./output/golden_t_hat.bin", dtype=np.int32).reshape(K_HAT, N)
        t = np.fromfile("./output/t_hat.bin", dtype=np.int32).reshape(K_HAT, N)
        rc |= check("t_hat vs golden (line 18)", t, g)

    encode_on = os.environ.get("HAT_BYTE_ENCODE", "0") == "1"

    if encode_on and not dot_only and os.path.isfile("./output/golden_ek_polyvec.bin") and os.path.isfile("./output/ek_polyvec.bin"):
        g = np.fromfile("./output/golden_ek_polyvec.bin", dtype=np.uint8)
        e = np.fromfile("./output/ek_polyvec.bin", dtype=np.uint8)
        if e.size > 0 and g.size > 0:
            rc |= check_bytes("ek_polyvec ByteEncode₁₂(t̂)", e, g)

    if encode_on and not dot_only and os.path.isfile("./output/golden_sk_polyvec.bin") and os.path.isfile("./output/sk_polyvec.bin"):
        g = np.fromfile("./output/golden_sk_polyvec.bin", dtype=np.uint8)
        s = np.fromfile("./output/sk_polyvec.bin", dtype=np.uint8)
        if s.size > 0 and g.size > 0:
            rc |= check_bytes("sk_polyvec ByteEncode₁₂(ŝ)", s, g)

    ek_pke_on = os.environ.get("F203_KEYGEN_EK_PKE", "1") == "1"
    if (
        ek_pke_on
        and encode_on
        and not dot_only
        and os.path.isfile("./output/golden_ek_pke.bin")
        and os.path.isfile("./output/ek_pke.bin")
    ):
        g = np.fromfile("./output/golden_ek_pke.bin", dtype=np.uint8)
        e = np.fromfile("./output/ek_pke.bin", dtype=np.uint8)
        if e.size > 0 and g.size > 0:
            rc |= check_bytes("ek_pke ek_polyvec||rho (line 21)", e, g)

    if rc == 0:
        print("[verify] PASS")
    return rc


if __name__ == "__main__":
    sys.exit(main())
