#!/usr/bin/env python3
"""verify_result — 对拍 K'/r' 与 golden（各 32B，max diff=0）。"""
from __future__ import annotations

import os
import sys

HALF = 32


def _cmp(name: str, got_path: str, golden_path: str) -> int:
    if not os.path.isfile(got_path):
        print(f"[verify] missing {got_path}", file=sys.stderr)
        return 1
    if not os.path.isfile(golden_path):
        print(f"[verify] missing {golden_path}", file=sys.stderr)
        return 1
    with open(got_path, "rb") as f:
        got = f.read()
    with open(golden_path, "rb") as f:
        golden = f.read()
    if len(got) != HALF or len(golden) != HALF:
        print(
            f"[verify] {name} size mismatch got={len(got)} golden={len(golden)}",
            file=sys.stderr,
        )
        return 1
    mism = sum(1 for a, b in zip(got, golden) if a != b)
    mx = max((abs(a - b) for a, b in zip(got, golden)), default=0)
    if mism != 0 or mx != 0:
        first = next(i for i, (a, b) in enumerate(zip(got, golden)) if a != b)
        print(
            f"[verify] FAIL {name} max={mx} nz={mism} first@{first} "
            f"got={got[first]:02x} golden={golden[first]:02x}",
            file=sys.stderr,
        )
        return 1
    print(f"[verify] PASS {name} max=0 ({HALF}B)")
    return 0


def main() -> None:
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    rc = 0
    rc |= _cmp(
        "K'",
        os.path.join(case, "output", "k_prime.bin"),
        os.path.join(case, "output", "golden_k_prime.bin"),
    )
    rc |= _cmp(
        "r'",
        os.path.join(case, "output", "r_prime.bin"),
        os.path.join(case, "output", "golden_r_prime.bin"),
    )
    if rc != 0:
        sys.exit(1)
    print("[verify] PASS K' and r' max=0")


if __name__ == "__main__":
    main()
