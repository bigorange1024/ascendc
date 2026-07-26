#!/usr/bin/env python3
"""verify_result — ByteDecode_d 探针：output/comp.bin 与 output/golden_comp.bin 逐系数对拍。

在流水线中的位置：main.cpp（CPU 孪生或 SIM/NPU）运行结束后，由 run.sh 调用本脚本，
比较 kernel 实际写出的 output/comp.bin 与 scripts/gen_data.py 生成的
output/golden_comp.bin，验证 FIPS 203 Alg.6 ByteDecode_d 语义正确性（I/O 等价，
不要求与参考实现逐行同构）。
"""
import os
import sys

import numpy as np

N = 256  # 单个多项式的系数个数，与 f203_mlkem_params.h 的 F203_MLKEM_N 保持一致。


def main() -> None:
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    out_path = os.path.join(case, "output", "comp.bin")
    golden_path = os.path.join(case, "output", "golden_comp.bin")

    if not os.path.isfile(out_path):
        print(f"[verify] missing {out_path}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(golden_path):
        print(f"[verify] missing {golden_path}", file=sys.stderr)
        sys.exit(1)

    got = np.fromfile(out_path, dtype=np.int32, count=N)
    golden = np.fromfile(golden_path, dtype=np.int32, count=N)
    if got.shape[0] != N or golden.shape[0] != N:
        print("[verify] size mismatch", file=sys.stderr)
        sys.exit(1)

    # 逐系数求绝对差；用 int64 避免 int32 减法在极端值下溢出。
    diff = np.abs(got.astype(np.int64) - golden.astype(np.int64))
    mx = int(diff.max())
    nz = int(np.count_nonzero(diff))
    if mx != 0:
        idx = int(np.argmax(diff))
        print(f"[verify] FAIL max={mx} nz={nz} first@{idx} got={got[idx]} golden={golden[idx]}", file=sys.stderr)
        sys.exit(1)
    d = int(os.environ.get("F203_BYTE_DECODE_D", "4"))
    print(f"[verify] PASS max=0 ({N} coeffs, d={d})")


if __name__ == "__main__":
    main()
