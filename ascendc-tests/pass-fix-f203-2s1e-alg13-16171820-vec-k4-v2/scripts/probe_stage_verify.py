#!/usr/bin/env python3
# coding=utf-8
"""阶段中间产物快速对拍（F203_PROBE_EARLY / 开发期手动调用）。

不必等全链路跑完再发现前段已错；每阶段只比前 HEAD_COEFFS=16 个系数。

用法（任意 mixPass 跑完后）：
  python3 scripts/probe_stage_verify.py

检查 output/ 里已有文件与 golden 的前若干系数/行；失败立即返回非 0。
与 pipeline_probe.hpp（CPU UB printf）互补：本脚本比 bin 文件头。
"""
import os
import sys

import numpy as np

N = 256
K_S = 4
K_E = 4
K_DST = 2 * K_S + K_E
M_ROWS = 32
HALF_N = N // 2
MAT_C_ROWS = 96
# 每阶段只比前几个系数，够定位「从哪一段开始偏」
HEAD_COEFFS = 16


def head_diff(label: str, got: np.ndarray, ref: np.ndarray, n: int) -> int:
    g = got.reshape(-1)[:n].astype(np.int64)
    r = ref.reshape(-1)[:n].astype(np.int64)
    d = np.abs(g - r)
    mx = int(d.max()) if d.size else 0
    if mx != 0:
        i = int(d.argmax())
        print(f"[PROBE_FAIL] {label} head{n} max_abs_diff={mx} idx={i} got={int(g[i])} ref={int(r[i])}")
        return 1
    print(f"[PROBE_OK] {label} head{n}")
    return 0


def main() -> int:
    rc = 0
    out = "./output"

    if os.path.isfile(f"{out}/golden_s0.bin") and os.path.isfile(f"{out}/s0.bin"):
        g = np.fromfile(f"{out}/golden_s0.bin", dtype=np.int8).reshape(M_ROWS, N)
        s = np.fromfile(f"{out}/s0.bin", dtype=np.int8).reshape(M_ROWS, N)
        rc |= head_diff("s0", s, g, HEAD_COEFFS)

    ck_s0 = f"{out}/_checkpoint_s0.bin"
    if os.path.isfile(f"{out}/golden_s0.bin") and os.path.isfile(ck_s0):
        g = np.fromfile(f"{out}/golden_s0.bin", dtype=np.int8).reshape(M_ROWS, N)
        s = np.fromfile(ck_s0, dtype=np.int8).reshape(M_ROWS, N)
        rc |= head_diff("checkpoint_s0", s, g, HEAD_COEFFS)

    if os.path.isfile(f"{out}/golden_mat_c.bin") and os.path.isfile(f"{out}/mat_c.bin"):
        g = np.fromfile(f"{out}/golden_mat_c.bin", dtype=np.int32).reshape(MAT_C_ROWS, HALF_N)
        c = np.fromfile(f"{out}/mat_c.bin", dtype=np.int32).reshape(MAT_C_ROWS, HALF_N)
        rc |= head_diff("mat_c_planar", c, g, HEAD_COEFFS)

    ck_mc = f"{out}/_checkpoint_mat_c.bin"
    if os.path.isfile(f"{out}/golden_mat_c.bin") and os.path.isfile(ck_mc):
        g = np.fromfile(f"{out}/golden_mat_c.bin", dtype=np.int32).reshape(MAT_C_ROWS, HALF_N)
        c = np.fromfile(ck_mc, dtype=np.int32).reshape(MAT_C_ROWS, HALF_N)
        rc |= head_diff("checkpoint_mat_c", c, g, HEAD_COEFFS)

    if os.path.isfile(f"{out}/golden.bin") and os.path.isfile(f"{out}/dst.bin"):
        g = np.fromfile(f"{out}/golden.bin", dtype=np.int32).reshape(K_DST, N)
        d = np.fromfile(f"{out}/dst.bin", dtype=np.int32).reshape(K_DST, N)
        rc |= head_diff("dst s_hat[0]", d[0], g[0], HEAD_COEFFS)
        rc |= head_diff("dst e_hat[0]", d[2 * K_S], g[2 * K_S], HEAD_COEFFS)

    dot_g = f"{out}/golden_t_hat_dot.bin"
    if os.path.isfile(dot_g) and os.path.isfile(f"{out}/t_hat.bin"):
        g = np.fromfile(dot_g, dtype=np.int32).reshape(K_S, N)
        t = np.fromfile(f"{out}/t_hat.bin", dtype=np.int32).reshape(K_S, N)
        rc |= head_diff("t_hat dot p=0", t[0], g[0], HEAD_COEFFS)

    if rc == 0:
        print("[probe_stage_verify] all available stages OK (head sample)")
    return rc


if __name__ == "__main__":
    sys.exit(main())
