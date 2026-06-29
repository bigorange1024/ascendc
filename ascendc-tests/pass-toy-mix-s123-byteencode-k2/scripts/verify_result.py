#!/usr/bin/env python3
"""
verify_result.py — 对比设备输出与 gen_data 生成的 golden。

按 mixPass 只校验本阶段会产生/更新的缓冲区：
  mixPass 0：out、s0、mat_c 全比
  mixPass 1：仅 s0（S1）
  mixPass 2：s0（preset 或前次跑）、mat_c（S2）
  mixPass 3：mat_c（preset）、out（S3+encode）

通过条件：各对比项 max_abs_diff == 0。
"""
import os
import sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "output")

K_S0 = 64 * 64


def max_abs_diff(a: np.ndarray, b: np.ndarray) -> int:
    return int(np.max(np.abs(a.astype(np.int32) - b.astype(np.int32))))


def main() -> int:
    mix_pass = int(os.environ.get("TOY_MIX_PASS", os.environ.get("PLANAR_MIX_PASS", "0")))

    if mix_pass in (0, 3):
        out = np.fromfile(os.path.join(OUT_DIR, "out.bin"), dtype=np.int8, count=K_S0)
        golden = np.fromfile(os.path.join(OUT_DIR, "golden_out.bin"), dtype=np.int8, count=K_S0)
        d = max_abs_diff(out, golden)
        print(f"out vs golden_out: max_abs_diff={d}")
        if d != 0:
            return 1

    if mix_pass in (0, 1, 2):
        s0 = np.fromfile(os.path.join(OUT_DIR, "s0.bin"), dtype=np.int8, count=K_S0)
        golden_s0 = np.fromfile(os.path.join(OUT_DIR, "golden_s0.bin"), dtype=np.int8, count=K_S0)
        d = max_abs_diff(s0, golden_s0)
        print(f"s0 vs golden_s0: max_abs_diff={d}")
        if d != 0:
            return 1

    if mix_pass in (0, 2, 3):
        mat_c = np.fromfile(os.path.join(OUT_DIR, "mat_c.bin"), dtype=np.int32, count=K_S0)
        golden_c = np.fromfile(os.path.join(OUT_DIR, "golden_mat_c.bin"), dtype=np.int32, count=K_S0)
        d = max_abs_diff(mat_c, golden_c)
        print(f"mat_c vs golden_mat_c: max_abs_diff={d}")
        if d != 0:
            return 1

    print("[verify] PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
