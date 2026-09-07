#!/usr/bin/env python3
# coding=utf-8
"""
gen_data.py — E04：ntt256 风格输入 + tiling(phase,tileLength)。

流水线：run.sh 编译后、kernel 前。
写 input/tiling.bin、src.bin、M4.bin 与 output/golden.bin。
语义：merged_kyber / ntt_sim_kyber 单 poly n=256；**≠ F203 Tag5T**。
"""
import os
import struct
import sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import ntt_sim_kyber  # noqa: E402

n = 256
np.random.seed(42)


def main() -> None:
    in_dir = os.path.join(ROOT, "input")
    out_dir = os.path.join(ROOT, "output")
    os.makedirs(in_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)

    # tiling: phase=0 占位 + tileLength=256，pad 到 64B
    payload = struct.pack("<ii", 0, n)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(in_dir, "tiling.bin"), "wb") as f:
        f.write(payload)

    input_x, golden = ntt_sim_kyber.gen_golden_data(n=n, q=3329, g=17)
    input_x = input_x.astype(np.int32)
    golden = golden.astype(np.int32)
    input_x.tofile(os.path.join(in_dir, "src.bin"))
    golden.tofile(os.path.join(out_dir, "golden.bin"))

    m = ntt_sim_kyber.M.astype(np.int32)
    m0 = ((m >> 0) & 0x7F).astype(np.int8).reshape(-1)
    m1 = ((m >> 7) & 0x7F).astype(np.int8).reshape(-1)
    m2 = ((m >> 14) & 0x7F).astype(np.int8).reshape(-1)
    m3 = ((m >> 21) & 0x7F).astype(np.int8).reshape(-1)
    m_out = np.concatenate((m0, m1, m2, m3)).astype(np.int8)
    m_out.tofile(os.path.join(in_dir, "M4.bin"))

    test01 = ntt_sim_kyber.ntt_test01(n=n, q=3329, g=17, f=input_x)
    assert np.all(test01 == golden)
    print(f"[gen_data] E04 ntt256-style I/O under {in_dir} (≠ Tag5T)")


if __name__ == "__main__":
    main()
