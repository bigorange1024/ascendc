#!/usr/bin/python3
# coding=utf-8
# EN02：分别对拍 NTT / INTT 两段 dst vs golden
# 失败由 run.sh 标 CORRECTNESS_SOFT_FAIL，不否决「不挂」门禁

import sys
import numpy as np

relative_tol = 0
absolute_tol = 0
error_tol = 0


def verify_one(label, output_path, golden_path):
    """对拍单段；打印 error ratio；返回是否通过。"""
    output = np.fromfile(output_path, dtype=np.int32).reshape(-1)
    golden = np.fromfile(golden_path, dtype=np.int32).reshape(-1)
    if output.size != golden.size:
        print(f"[{label}] size mismatch: out={output.size} golden={golden.size}")
        return False
    close = np.isclose(output, golden, rtol=relative_tol, atol=absolute_tol, equal_nan=True)
    bad = np.where(close == False)[0]
    for index in range(min(len(bad), 32)):
        i = bad[index]
        print(
            f"[{label}] idx={i:06d} expected={golden[i]}({golden[i]:x}) "
            f"actual={output[i]}({output[i]:x}) diff={output[i]-golden[i]}"
        )
    error_ratio = float(bad.size) / float(golden.size) if golden.size else 1.0
    print(f"[{label}] error ratio: {error_ratio:.4f}, tolerance: {error_tol:.4f}, bad={bad.size}")
    return error_ratio <= error_tol


if __name__ == "__main__":
    # 无参：默认 EN02 两段路径；亦可 argv 传单对
    try:
        if len(sys.argv) >= 3:
            ok = verify_one("custom", sys.argv[1], sys.argv[2])
            if not ok:
                raise ValueError("[ERROR] result error")
            print("test pass")
        else:
            ok_ntt = verify_one("NTT", "output/dst_ntt.bin", "output/golden_ntt.bin")
            ok_intt = verify_one("INTT", "output/dst_intt.bin", "output/golden_intt.bin")
            if not (ok_ntt and ok_intt):
                raise ValueError("[ERROR] NTT and/or INTT golden mismatch")
            print("test pass (NTT+INTT)")
    except Exception as e:
        print(e)
        sys.exit(1)
