#!/usr/bin/env python3
"""roundtrip_kem_verify.py — KEM 纯 device 闭环断言（不依赖 liboqs）。

两个子模式：
- agree  ：断言 device Encaps K == device Decaps K'（ML-KEM 共享秘密一致性，闭环核心）。
- reject ：断言 device 拒绝路径 K == J(z‖c)=SHAKE256(z‖c,32) 且 != Encaps K。
           z 取自 dk_kem 尾 32B（dk[3136:3168]），c 为送入 Decaps 的密文。

golden 完全由 device 输出 + FIPS 203 定义的 J 自算，无需外部参考实现。
"""
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

SS_BYTES = 32
DK_BYTES = 3168


def _read(path: Path) -> bytes:
    return Path(path).read_bytes()


def _max_diff(a: bytes, b: bytes) -> int:
    if len(a) != len(b):
        return 256
    return max((abs(x - y) for x, y in zip(a, b)), default=0)


def cmd_agree(args) -> int:
    """Encaps K 与 Decaps K' 逐字节一致才 PASS。"""
    k_enc = _read(args.k_enc)
    k_dec = _read(args.k_dec)
    mx = _max_diff(k_enc, k_dec)
    if mx != 0:
        print(f"[roundtrip_kem] FAIL agreement: Encaps K vs Decaps K' max={mx}")
        return 1
    print(f"[roundtrip_kem] PASS agreement: Encaps K == Decaps K' ({len(k_enc)} bytes)")
    return 0


def cmd_reject(args) -> int:
    """拒绝路径：K 必须等于 J(z‖c) 且区别于合法 Encaps K。"""
    dk = _read(args.dk)
    c = _read(args.c)
    k = _read(args.k)
    if len(dk) != DK_BYTES:
        print(f"[roundtrip_kem] FAIL reject: dk size {len(dk)} != {DK_BYTES}")
        return 1
    z = dk[3136:3168]  # FIPS 203 dk_kem 尾部隐式拒绝秘密 z
    j_zc = hashlib.shake_256(z + c).digest(SS_BYTES)
    rc = 0
    mx = _max_diff(k, j_zc)
    if mx != 0:
        print(f"[roundtrip_kem] FAIL reject: K vs J(z||c) max={mx}")
        rc = 1
    else:
        print(f"[roundtrip_kem] PASS reject: K == J(z||c) ({SS_BYTES} bytes)")
    if args.k_enc is not None:
        k_enc = _read(args.k_enc)
        if _max_diff(k, k_enc) == 0:
            print("[roundtrip_kem] FAIL reject: 拒绝秘密与合法 Encaps K 相同")
            rc = 1
        else:
            print("[roundtrip_kem] PASS reject: K != Encaps K")
    return rc


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_agree = sub.add_parser("agree")
    p_agree.add_argument("--k-enc", type=Path, required=True)
    p_agree.add_argument("--k-dec", type=Path, required=True)
    p_agree.set_defaults(func=cmd_agree)

    p_reject = sub.add_parser("reject")
    p_reject.add_argument("--dk", type=Path, required=True)
    p_reject.add_argument("--c", type=Path, required=True)
    p_reject.add_argument("--k", type=Path, required=True)
    p_reject.add_argument("--k-enc", type=Path, default=None)
    p_reject.set_defaults(func=cmd_reject)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
