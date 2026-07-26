#!/usr/bin/env python3
"""verify_result — 对拍设备输出 output/poly.bin 与 golden output/golden_poly.bin。

本文件在流水线中的位置：run.sh 在 kernel（CPU 或 SIM）执行完成后调用本脚本，做最终的
I/O 等价判定；只比较逐系数差值是否全 0，不关心设备侧内部实现是向量还是标量路径。
"""
import os
import sys

import numpy as np

N = 256  # 单个多项式系数个数，须与 gen_data.py / kernel 侧一致


def main() -> None:
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    out_path = os.path.join(case, "output", "poly.bin")
    golden_path = os.path.join(case, "output", "golden_poly.bin")
    if not os.path.isfile(out_path):
        print(f"[verify] missing {out_path}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(golden_path):
        print(f"[verify] missing {golden_path}", file=sys.stderr)
        sys.exit(1)

    # 按 int32 读取前 N 个元素（对应单个多项式的 canonical 系数）。
    got = np.fromfile(out_path, dtype=np.int32, count=N)
    golden = np.fromfile(golden_path, dtype=np.int32, count=N)
    if got.shape[0] != N or golden.shape[0] != N:
        print("[verify] size mismatch", file=sys.stderr)
        sys.exit(1)

    # 逐系数绝对差值；用 int64 计算避免 int32 减法在极端情况下溢出。
    diff = np.abs(got.astype(np.int64) - golden.astype(np.int64))
    mx = int(diff.max())
    nz = int(np.count_nonzero(diff))
    if mx != 0:
        # 定位第一个（也是最大差值处的）不一致系数，便于排错。
        idx = int(np.argmax(diff))
        print(f"[verify] FAIL max={mx} nz={nz} first@{idx} got={got[idx]} golden={golden[idx]}", file=sys.stderr)
        sys.exit(1)
    print(f"[verify] PASS max=0 ({N} coeffs)")


if __name__ == "__main__":
    main()
