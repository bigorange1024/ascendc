#!/usr/bin/env python3
"""verify_result — RB-D06：合法+拒绝 K 对拍 liboqs golden；拒绝≠合法。

权威：output/cross_backend.txt 必须为 liboqs；缺 BLOCKED / 非 liboqs → FAIL（勿假绿）。
假绿三问书面答见 STATUS / FEEDBACK。
"""
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
    out = os.path.join(case, "output")

    blocked = os.path.join(out, "BLOCKED")
    if os.path.isfile(blocked):
        print(f"[verify] BLOCKED: {open(blocked, encoding='utf-8').read().strip()}", file=sys.stderr)
        sys.exit(2)

    backend_path = os.path.join(out, "cross_backend.txt")
    if not os.path.isfile(backend_path):
        print("[verify] missing cross_backend.txt", file=sys.stderr)
        sys.exit(1)
    backend = open(backend_path, encoding="utf-8").read().strip()
    if backend != "liboqs":
        print(f"[verify] FAIL 非权威 backend={backend!r}（须 liboqs）", file=sys.stderr)
        sys.exit(1)

    rc = 0
    rc |= _cmp(
        "K_legit",
        os.path.join(out, "k_legit.bin"),
        os.path.join(out, "golden_k_legit.bin"),
    )
    rc |= _cmp(
        "K_reject",
        os.path.join(out, "k_reject.bin"),
        os.path.join(out, "golden_k_reject.bin"),
    )
    if rc != 0:
        sys.exit(1)

    with open(os.path.join(out, "k_legit.bin"), "rb") as f:
        k_l = f.read()
    with open(os.path.join(out, "k_reject.bin"), "rb") as f:
        k_r = f.read()
    if k_l == k_r:
        print(
            "[verify] FAIL 拒绝路径 K 与合法相同（疑恒输出 K' 假绿）",
            file=sys.stderr,
        )
        sys.exit(1)
    print("[verify] PASS K_reject != K_legit")
    print("[verify] PASS FO both paths vs liboqs Decaps max=0")


if __name__ == "__main__":
    main()
