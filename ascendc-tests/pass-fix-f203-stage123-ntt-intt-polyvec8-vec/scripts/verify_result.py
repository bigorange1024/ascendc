#!/usr/bin/env python3
import os
import sys

import numpy as np

K = 8
N = 256


def main() -> None:
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    out_path = os.path.join(case, "output", "dst.bin")
    golden_path = os.path.join(case, "output", "golden_dst.bin")
    if not os.path.isfile(out_path):
        print(f"[verify] missing {out_path}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(golden_path):
        print(f"[verify] missing {golden_path}", file=sys.stderr)
        sys.exit(1)

    got = np.fromfile(out_path, dtype=np.int32, count=K * N).reshape(K, N)
    golden = np.fromfile(golden_path, dtype=np.int32, count=K * N).reshape(K, N)
    diff = np.abs(got.astype(np.int64) - golden.astype(np.int64))
    mx = int(diff.max())
    nz = int(np.count_nonzero(diff))
    if mx != 0:
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
