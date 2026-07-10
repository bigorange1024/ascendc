#!/usr/bin/env python3
"""verify_result — ByteEncode_d 探针：output/encoded.bin 与 output/golden_encoded.bin 逐字节对拍。

在流水线中的位置：main.cpp（CPU 孪生或 SIM/NPU）运行结束后，由 run.sh 调用本脚本，
比较 kernel 实际写出的 output/encoded.bin 与 scripts/gen_data.py 生成的
output/golden_encoded.bin，验证 FIPS 203 Alg.5 ByteEncode_d 语义正确性（I/O 等价，
不要求与参考实现逐行同构）。
"""
import os
import sys

import numpy as np

# 各 d 值对应的编码输出字节数，须与 byte_encode_d_config.hpp 的 F203_BYTE_ENCODE_POLY_BYTES 一致。
OUT_BYTES = {4: 128, 5: 160, 10: 320, 11: 352}


def main() -> None:
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    # d 由环境变量 F203_BYTE_ENCODE_D 决定，须与本次 run.sh 编译 kernel 时使用的 d 一致。
    d = int(os.environ.get("F203_BYTE_ENCODE_D", "4"))
    if d not in OUT_BYTES:
        print("[verify] F203_BYTE_ENCODE_D must be 4, 5, 10, or 11", file=sys.stderr)
        sys.exit(1)

    out_path = os.path.join(case, "output", "encoded.bin")
    golden_path = os.path.join(case, "output", "golden_encoded.bin")
    nbytes = OUT_BYTES[d]

    if not os.path.isfile(out_path):
        print(f"[verify] missing {out_path}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(golden_path):
        print(f"[verify] missing {golden_path}", file=sys.stderr)
        sys.exit(1)

    got = np.fromfile(out_path, dtype=np.uint8, count=nbytes)
    golden = np.fromfile(golden_path, dtype=np.uint8, count=nbytes)
    if got.shape[0] != nbytes or golden.shape[0] != nbytes:
        print("[verify] size mismatch", file=sys.stderr)
        sys.exit(1)

    # 逐字节求绝对差；用 int16 避免 uint8 减法下溢（差值理论上限为 255，int16 足够容纳）。
    diff = np.abs(got.astype(np.int16) - golden.astype(np.int16))
    mx = int(diff.max())
    nz = int(np.count_nonzero(diff))
    if mx != 0:
        idx = int(np.argmax(diff))
        print(f"[verify] FAIL max={mx} nz={nz} first@{idx} got={got[idx]} golden={golden[idx]}", file=sys.stderr)
        sys.exit(1)
    print(f"[verify] PASS max=0 ({nbytes} bytes, d={d})")


if __name__ == "__main__":
    main()
