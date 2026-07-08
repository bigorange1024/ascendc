#!/usr/bin/python3
"""分阶段验收：verify_stage.py <stage>"""
import os
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _SCRIPT_DIR)

from mlkem_ref import (  # noqa: E402
    K,
    M_MAT_A,
    N,
    OUT_COLS,
    encode_mat_a,
    f203_stage3_route_a,
    f203_three_stage_batch,
    gen_fixed_se_polyvec,
    load_mat_b_lut_i8,
    matmul_int8_i32,
    stage31_mod,
)


def verify_stage1():
    se = gen_fixed_se_polyvec()
    golden = encode_mat_a(se)
    got = np.fromfile("./output/stage1_mata.bin", dtype=np.int8).reshape(M_MAT_A, N)
    diff = int(np.max(np.abs(golden.astype(np.int16) - got.astype(np.int16))))
    print(f"stage1 pass (max_abs_diff={diff})")
    return diff == 0


def verify_stage2():
    se = gen_fixed_se_polyvec()
    mat_a = encode_mat_a(se)
    mat_b = load_mat_b_lut_i8()
    golden = matmul_int8_i32(mat_a, mat_b)
    got = np.fromfile("./output/stage2_matc.bin", dtype=np.int32).reshape(M_MAT_A, OUT_COLS)
    diff = int(np.max(np.abs(golden.astype(np.int64) - got.astype(np.int64))))
    print(f"stage2 pass (max_abs_diff={diff})")
    return diff == 0


def verify_stage3():
    golden = f203_three_stage_batch(gen_fixed_se_polyvec())
    got = np.fromfile("./output/output.bin", dtype=np.int32).reshape(K, N)
    diff = int(np.max(np.abs(golden.astype(np.int64) - got.astype(np.int64))))
    print(f"stage3 pass (max_abs_diff={diff})")
    return diff == 0


def main():
    stage = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    os.chdir(os.path.dirname(_SCRIPT_DIR))
    if stage == 1:
        ok = verify_stage1()
    elif stage == 2:
        ok = verify_stage2()
    elif stage == 3:
        ok = verify_stage3()
    else:
        raise SystemExit(f"unknown stage {stage}")
    raise SystemExit(0 if ok else 1)


if __name__ == "__main__":
    main()
