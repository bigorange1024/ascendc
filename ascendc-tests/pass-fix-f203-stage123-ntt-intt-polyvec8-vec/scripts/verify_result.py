#!/usr/bin/env python3
"""
@file verify_result.py
@brief 本探针验收：逐元素比对 output/dst.bin 与 golden_dst.bin。

流水线位置：run.sh 在 kernel 成功写出 dst 后调用；不参与 gen_data / 设备计算。

作用：读 [8,256] int32，统计 max|diff| 与非零个数；max!=0 则 FAIL exit 1。
与 golden 关系：仅 I/O 等价验收；打印 F203_NTT_MODE 便于区分 NTT/INTT 日志。
"""
import os
import sys

import numpy as np

K = 8
N = 256


def main() -> None:
    """
    对拍入口。
    前置：output/dst.bin 与 output/golden_dst.bin 均存在且各含 K*N 个 int32。
    """
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    out_path = os.path.join(case, "output", "dst.bin")
    golden_path = os.path.join(case, "output", "golden_dst.bin")
    if not os.path.isfile(out_path):
        print(f"[verify] missing {out_path}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(golden_path):
        print(f"[verify] missing {golden_path}", file=sys.stderr)
        sys.exit(1)

    # 固定 count 防止尾随脏数据；reshape 为 polyvec
    got = np.fromfile(out_path, dtype=np.int32, count=K * N).reshape(K, N)
    golden = np.fromfile(golden_path, dtype=np.int32, count=K * N).reshape(K, N)
    diff = np.abs(got.astype(np.int64) - golden.astype(np.int64))
    mx = int(diff.max())
    nz = int(np.count_nonzero(diff))
    if mx != 0:
        # 报告首个最大误差位置，便于分段排查
        idx = int(np.unravel_index(int(np.argmax(diff)), diff.shape))
        print(
            f"[verify] FAIL max={mx} nz={nz} first@{idx} got={got[idx]} golden={golden[idx]}",
            file=sys.stderr,
        )
        sys.exit(1)
    mode = os.environ.get("F203_NTT_MODE", "ntt")
    print(f"[verify] PASS max=0 ({K}x{N}, mode={mode})")


if __name__ == "__main__":
    main()
