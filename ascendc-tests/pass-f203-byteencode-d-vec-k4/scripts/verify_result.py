#!/usr/bin/env python3
import os
import sys

import numpy as np

OUT_BYTES = {4: 128, 5: 160, 10: 320, 11: 352}


def main() -> None:
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
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
