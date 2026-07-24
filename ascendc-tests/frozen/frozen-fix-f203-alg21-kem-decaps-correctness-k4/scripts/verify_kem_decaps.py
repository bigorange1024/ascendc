#!/usr/bin/env python3
"""
verify_kem_decaps.py — Alg.21 Decaps 端到端 I/O 对拍。

合法路径：output/K.bin vs golden_K.bin（来自 Alg.20 Encaps）。
拒绝路径（KEM_DECAPS_TAMPER_C=1）：K vs J(z‖c)，且不得仍等于 encaps K。
仅验字节等价；FO 本身在设备 kem_dec_pack 完成。
"""
from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path


def max_diff(a: bytes, b: bytes) -> int:
    """逐字节最大绝对差；长度不等时至少返回 256。"""
    n = min(len(a), len(b))
    m = 0
    for i in range(n):
        m = max(m, abs(a[i] - b[i]))
    if len(a) != len(b):
        return max(m, 256)
    return m


def main() -> None:
    """按是否篡改密文选择合法/拒绝 golden。"""
    root = Path(__file__).resolve().parent.parent
    out = root / "output"
    inp = root / "input"
    k = (out / "K.bin").read_bytes()
    tamper = os.environ.get("KEM_DECAPS_TAMPER_C", "0") == "1"

    if tamper:
        reject_path = out / "golden_K_reject.bin"
        if not reject_path.is_file():
            dk = (inp / "dk_kem.bin").read_bytes()
            c = (inp / "c.bin").read_bytes()
            z = dk[3136:3168]
            reject_path.write_bytes(hashlib.shake_256(z + c).digest(32))
        gk = reject_path.read_bytes()
        mk = max_diff(k, gk)
        mk_enc = max_diff(k, (out / "golden_K.bin").read_bytes())
        print(f"[verify_kem_decaps] reject path K vs J(z||c) max={mk}")
        print(f"[verify_kem_decaps] reject path K vs encaps K max={mk_enc}")
        if mk != 0:
            sys.exit(1)
        if mk_enc == 0:
            print("[verify_kem_decaps] FAIL: tampered c but K still equals encaps K")
            sys.exit(2)
        print("[verify_kem_decaps] REJECT PASS")
        return

    gk = (out / "golden_K.bin").read_bytes()
    mk = max_diff(k, gk)
    print(f"[verify_kem_decaps] K max={mk}")
    if mk != 0:
        sys.exit(1)
    print("[verify_kem_decaps] PASS")


if __name__ == "__main__":
    main()
