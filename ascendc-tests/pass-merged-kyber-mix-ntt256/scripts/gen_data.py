#!/usr/bin/python3
# coding=utf-8
"""pass-merged-kyber-mix-ntt256 输入与 golden 生成。

流水线位置：run.sh 编译后、kernel 前调用。
作用：写 input/tiling.bin、src.bin、M4.bin 与 output/golden.bin。
语义：单 poly n=256；golden 来自 ntt_sim_kyber（Barrett 路径），仅作 I/O 对拍。
来源：原 thirdparty/merged_kyber/scripts，授权迁入本用例。
"""
import numpy as np
import os
import ntt_sim_kyber

n = 256
np.random.seed(42)

def gen_tiling():
    os.system("mkdir -p input")
    os.system("mkdir -p output")
    tiling_data = [n]
    tiling_np = np.array(tiling_data, dtype=np.int32)
    tiling_np.tofile("./input/tiling.bin")


if __name__ == "__main__":
    gen_tiling()
    (input_x, golden) = ntt_sim_kyber.gen_golden_data(n=n, q=3329, g=17)
    input_x = input_x.astype(np.int32)
    golden = golden.astype(np.int32)

    input_x.tofile("./input/src.bin")
    m = ntt_sim_kyber.M.astype(np.int32)
    golden.tofile("./output/golden.bin")

    print(m)
    m0 = ((m >> 0 ) & 0x7f).astype(np.int8).reshape(-1)
    m1 = ((m >> 7 ) & 0x7f).astype(np.int8).reshape(-1)
    m2 = ((m >> 14) & 0x7f).astype(np.int8).reshape(-1)
    m3 = ((m >> 21) & 0x7f).astype(np.int8).reshape(-1)
    print(np.max(m0), np.max(m1), np.max(m2), np.max(m3))
    m_out = np.concatenate((m0, m1, m2, m3), dtype=np.int8)
    print("m_out size =", len(m_out))
    print(m_out)
    m_out.tofile("./input/M4.bin")

    test01 = ntt_sim_kyber.ntt_test01(n=n, q=3329, g=17, f=input_x)
    assert np.all(test01 == golden)

    nomod = ntt_sim_kyber.ntt_test01_nomod(n=n, q=3329, g=17, f=input_x).astype(np.int32)
    nomod.tofile("./output/nomod.bin")
