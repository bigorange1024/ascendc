#!/usr/bin/env python3
"""
verify_kem_decaps.py — 对拍 output/K.bin。

合法：vs golden/K.bin（liboqs encaps）。
拒绝（KEM_DECAPS_REJECT=1 或 golden/mode_reject）：vs liboqs Decaps ≡ J(z‖c)；
  并断言 ≠ 同 dk 下合法 encaps 的 K（若 golden/K_accept.bin 存在则比）。

说明：liboqs Decaps API 只返回 K，不暴露内部重加密 c'；E3 验收对象是最终共享密钥。
"""
from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def max_diff(a: bytes, b: bytes) -> int:
    n = min(len(a), len(b))
    m = max((abs(a[i] - b[i]) for i in range(n)), default=0)
    return max(m, 256) if len(a) != len(b) else m


def main() -> int:
    out = ROOT / "output" / "K.bin"
    g = ROOT / "golden" / "K.bin"
    if not out.is_file() or not g.is_file():
        print(f"[verify] missing {out} or {g}", file=sys.stderr)
        return 2

    ob, gb = out.read_bytes(), g.read_bytes()
    if len(ob) != 32 or len(gb) != 32:
        print(f"[verify] bad size out={len(ob)} golden={len(gb)}", file=sys.stderr)
        return 1

    reject = os.environ.get("KEM_DECAPS_REJECT", "0") == "1" or (ROOT / "golden" / "mode_reject").is_file()
    d = max_diff(ob, gb)
    print(f"[verify] K.bin max_abs_diff={d}")

    if reject:
        dk = (ROOT / "input" / "dk_kem.bin").read_bytes()
        c = (ROOT / "input" / "c.bin").read_bytes()
        z = dk[3136:3168]
        j = hashlib.shake_256(z + c).digest(32)
        dj = max_diff(ob, j)
        print(f"[verify] K vs J(z||c) max_abs_diff={dj}")
        if d != 0 or dj != 0:
            print("[verify] REJECT FAIL")
            return 1
        # 与「同密钥合法 encaps」区分（可选；本轮 gen 未写 K_accept 则跳过）
        k_acc = ROOT / "golden" / "K_accept.bin"
        if k_acc.is_file() and max_diff(ob, k_acc.read_bytes()) == 0:
            print("[verify] REJECT FAIL: K still equals accept-path K")
            return 2
        print("[verify] REJECT PASS (device K == liboqs Decaps == J(z||c))")
        return 0

    print("[verify] PASS" if d == 0 else "[verify] FAIL")
    return 0 if d == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
