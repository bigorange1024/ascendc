#!/usr/bin/env python3
"""liboqs_kem_vs_ascendc_verify.py — liboqs fixture vs AscendC KEM 逐级对拍。

按 --stage 选择比对项：
- keygen ：AscendC ek_kem/dk_kem  vs fixture ek_kem/dk_kem
- encaps ：AscendC c/K            vs fixture c/K
- decaps ：AscendC K              vs fixture K_decaps（并断言 == fixture K，即 shared-secret agreement）
- reject ：AscendC K              vs fixture K_reject（并断言 != fixture K，确认走隐式拒绝）

约定：所有对拍均逐字节 max=0 才算 PASS；shape 不符直接 FAIL。
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np


def _max_diff(got: Path, ref: Path) -> tuple[int, int, int]:
    """返回 (max_abs_diff, got_size, ref_size)；shape 不符时 max_abs_diff=256（哨兵）。"""
    g = np.fromfile(got, dtype=np.uint8)
    r = np.fromfile(ref, dtype=np.uint8)
    if g.shape != r.shape:
        return 256, g.size, r.size
    if g.size == 0:
        return 0, 0, 0
    return int(np.max(np.abs(g.astype(np.int16) - r.astype(np.int16)))), g.size, r.size


def _cmp(label: str, got: Path, ref: Path) -> int:
    """逐字节对拍并打印结果；PASS 返回 0，FAIL 返回 1。"""
    mx, gs, rs = _max_diff(got, ref)
    if mx != 0:
        print(f"[liboqs_kem_vs] FAIL {label} max={mx} (got {gs}B vs ref {rs}B)")
        return 1
    print(f"[liboqs_kem_vs] PASS {label} max=0 ({gs} bytes)")
    return 0


def _assert_equal(label: str, a: Path, b: Path, want_equal: bool) -> int:
    """断言两文件字节是否相等（用于 K 一致性 / 拒绝路径差异性）。"""
    mx, _, _ = _max_diff(a, b)
    equal = mx == 0
    if equal == want_equal:
        rel = "==" if want_equal else "!="
        print(f"[liboqs_kem_vs] PASS {label} ({a.name} {rel} {b.name})")
        return 0
    rel = "==" if want_equal else "!="
    print(f"[liboqs_kem_vs] FAIL {label} expected {a.name} {rel} {b.name} but max={mx}")
    return 1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--stage", required=True, choices=["keygen", "encaps", "decaps", "reject"])
    ap.add_argument("--fixture-dir", type=Path, required=True)
    ap.add_argument("--ascendc-out", type=Path, required=True)
    args = ap.parse_args()

    fx = args.fixture_dir
    ao = args.ascendc_out
    rc = 0

    if args.stage == "keygen":
        rc |= _cmp("ek_kem", ao / "ek_kem.bin", fx / "ek_kem.bin")
        rc |= _cmp("dk_kem", ao / "dk_kem.bin", fx / "dk_kem.bin")
    elif args.stage == "encaps":
        rc |= _cmp("c", ao / "c.bin", fx / "c.bin")
        rc |= _cmp("K", ao / "K.bin", fx / "K.bin")
    elif args.stage == "decaps":
        # device Decaps(合法 c) 的 K 应同时等于 fixture 的 Decaps K' 与 Encaps K（shared-secret agreement）。
        rc |= _cmp("K(decaps)", ao / "K.bin", fx / "K_decaps.bin")
        rc |= _assert_equal("shared-secret agreement", fx / "K_decaps.bin", fx / "K.bin", want_equal=True)
    elif args.stage == "reject":
        # device 拒绝路径 K 应等于 fixture 拒绝秘密 J(z‖c_bad)，且区别于合法秘密 K。
        rc |= _cmp("K(reject)", ao / "K.bin", fx / "K_reject.bin")
        rc |= _assert_equal("reject != accept", fx / "K_reject.bin", fx / "K.bin", want_equal=False)

    return rc


if __name__ == "__main__":
    sys.exit(main())
