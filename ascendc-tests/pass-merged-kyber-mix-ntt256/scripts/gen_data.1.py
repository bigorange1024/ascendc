#!/usr/bin/python3
# coding=utf-8
from ntt_sim_kyber import *

if __name__ == "__main__":
    # np.set_printoptions(threshold=np.inf, linewidth=np.inf)
    (input_x, golden) = gen_golden_data(NTT_N, NTT_Q, NTT_G, TOTAL_LENGTH, TILE_NUM, CORE_NUM)
    M.tofile("./input/M.bin")
    input_x.tofile("./input/input_x.bin")
    golden.tofile("./output/golden.bin")
