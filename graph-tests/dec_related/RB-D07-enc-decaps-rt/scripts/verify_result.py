#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""verify_result — RB-D07：K_dec ≡ K_enc ≡ liboqs golden_k（max=0）。

权威：output/cross_backend.txt 必须为 liboqs；缺 BLOCKED / 非 liboqs → FAIL。
假绿三问书面答见 STATUS / FEEDBACK。
"""
from __future__ import annotations

import os
import sys

HALF = 32
CT = 1568


def _cmp(name: str, got_path: str, golden_path: str, nbytes: int) -> int:
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
    if len(got) != nbytes or len(golden) != nbytes:
        print(
            f"[verify] {name} size mismatch got={len(got)} golden={len(golden)} need={nbytes}",
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
    print(f"[verify] PASS {name} max=0 ({nbytes}B)")
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
    # 主验收：往返 K 闭合
    rc |= _cmp("K_enc", os.path.join(out, "k_enc.bin"), os.path.join(out, "golden_k.bin"), HALF)
    rc |= _cmp("K_dec", os.path.join(out, "k_dec.bin"), os.path.join(out, "golden_k.bin"), HALF)
    if rc != 0:
        sys.exit(1)

    with open(os.path.join(out, "k_enc.bin"), "rb") as f:
        k_enc = f.read()
    with open(os.path.join(out, "k_dec.bin"), "rb") as f:
        k_dec = f.read()
    if k_enc != k_dec:
        print("[verify] FAIL K_dec != K_enc（往返未闭合）", file=sys.stderr)
        sys.exit(1)
    print("[verify] PASS K_dec ≡ K_enc ≡ liboqs")

    # 诊断：设备 c 与 liboqs Encaps c（非硬门；错则告警）
    c_path = os.path.join(out, "c_enc.bin")
    gc_path = os.path.join(out, "golden_c.bin")
    if os.path.isfile(c_path) and os.path.isfile(gc_path):
        diag = _cmp("c_enc(diag)", c_path, gc_path, CT)
        if diag != 0:
            print("[verify] WARN c_enc≠golden_c（诊断；主验收已以 K 为准）", file=sys.stderr)

    print("[verify] PASS RT K_dec≡K_enc≡liboqs max=0")


if __name__ == "__main__":
    main()
