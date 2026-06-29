#!/usr/bin/env python3
"""生成 G2 NTT + G4 INTT even/odd stacked LUT bin（写入 input/）。"""
from __future__ import annotations

import re
import sys
from pathlib import Path

import numpy as np

N = 256
CASE = Path(__file__).resolve().parent.parent.parent
LUT_HDR = CASE / "compute/ntt_r/thirdparty/ntt_study/include/mlkem/stable/transpose_mlkem_luts_i8.h"


def load_lut_t_i8(mode: str) -> np.ndarray:
    symbol = "kMlkemLimb6Ntt_T_i8" if mode == "ntt" else "kMlkemLimb6Intt_T_i8"
    txt = LUT_HDR.read_text(encoding="utf-8")
    i0 = txt.index(symbol)
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    nums = [int(x) for x in re.findall(r"-?\d+", txt[i1 + 1 : i2])]
    return np.array(nums, dtype=np.int8).reshape(N, 512)


def lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def write_bins(out_dir: Path) -> None:
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    lut_even = lut_planar_stacked(lut_ntt, True)
    lut_odd = lut_planar_stacked(lut_ntt, False)
    intt_even = lut_planar_stacked(lut_intt, True)
    intt_odd = lut_planar_stacked(lut_intt, False)
    out_dir.mkdir(parents=True, exist_ok=True)
    lut_even.tofile(out_dir / "lut_even_stacked.bin")
    lut_odd.tofile(out_dir / "lut_odd_stacked.bin")
    intt_even.tofile(out_dir / "lut_intt_even_stacked.bin")
    intt_odd.tofile(out_dir / "lut_intt_odd_stacked.bin")
    print(
        f"[ntt_lut_bins] ntt_even={lut_even.nbytes}B intt_even={intt_even.nbytes}B -> {out_dir}"
    )


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else CASE / "input"
    write_bins(out)


if __name__ == "__main__":
    main()
