#!/usr/bin/env python3
"""
gate_g3.py — Decrypt 分段门禁 G3：golden ŵ = ⟨ŝ, û⟩（Alg.11 累加）。

用法：gate_g3.py <case_dir> <out_dir> → golden_w_hat.bin
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from f203_ref_common import stage123_transform
from golden_m import decode_s_hat, golden_w_hat, unpack_ciphertext


def main() -> None:
    case = Path(sys.argv[1])
    out = Path(sys.argv[2])
    dk = bytes(np.fromfile(case / "input/dk_pke.bin", dtype=np.uint8))
    c = bytes(np.fromfile(case / "input/c.bin", dtype=np.uint8))
    s_hat = decode_s_hat(dk)
    u, _ = unpack_ciphertext(c)
    u_hat = stage123_transform(u, "ntt")
    w_hat = golden_w_hat(s_hat, u_hat)
    w_hat.astype(np.int32).tofile(out / "golden_w_hat.bin")
    print("[gate_g3] w_hat golden OK")


if __name__ == "__main__":
    main()
