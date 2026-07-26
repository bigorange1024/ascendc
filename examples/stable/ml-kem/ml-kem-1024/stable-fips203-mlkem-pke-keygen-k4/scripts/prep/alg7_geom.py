from __future__ import annotations
# @probe stable-fips203-mlkem-pke-keygen-k4
# @file scripts/prep/alg7_geom.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `alg7_geom.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

# coding=utf-8
"""
本文件在 KeyGen 流水线中的位置：Launch 1 Alg.7 SampleNTT（Â）：XOF/拒绝采样/ROM。
对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
文件：scripts/prep/alg7_geom.py
"""
"""Alg.7 SampleNTT 几何常量（Python 侧单一真源，须与 f203_alg7_layout.h 逐行同步）。

本模块定义 XOF 预 squeeze 长度、候选对数、交错流长度等**与实现无关**的算术关系。
任何修改必须同时更新：
  - f203_alg7_layout.h（设备/host C++ 编译期常量）
  - scripts/gen_alg7_*_rom.py、gen_data.py、verify_result.py 中的引用

几何推导（见 f203_alg7_layout.h 文件头）：
  - SHAKE128 rate = 168B；Kyber 首批启发式 3×rate = 504B
  - 本探针固定再 squeeze 1×rate（672B 总），不做 lazy tail while
  - 每 3 字节 (c0,c1,c2) 解出一个 (d1,d2) 候选对 → 672/3 = 224 对
  - rej 按规范顺序扫描 d1[0],d2[0],d1[1],d2[1],… → 448 个 stream lane
"""

import os

# SHAKE128 单次 squeeze 输出块长（Keccak-f[1600] rate，与 Kyber/mlkem 一致）
SHAKE128_RATE = 168

# Kyber GEN_MATRIX 首批块数（启发式 3×168B = 504B，可产生 336 组三字节候选）
GEN_MATRIX_NBLOCKS = 3

# 504B 实验：F203_ALG7_XOF_504=1 时仅 3×rate（须与 f203_alg7_layout.h 同步）
_XOF_504 = os.environ.get("F203_ALG7_XOF_504", "0") == "1"
# 固定预 squeeze 的额外 rate 块数（= 业界 tail 单次增量 168B；本探针不做 while 续流）
TAIL_PREFETCH_BLOCKS = 0 if _XOF_504 else 1

# 一次 absorb 上连续 squeeze 的 rate 块总数：3 + 1 = 4
XOF_SQUEEZE_BLOCKS = GEN_MATRIX_NBLOCKS + TAIL_PREFETCH_BLOCKS

# XOF 总字节数：4 × 168 = 672（32B 对齐，便于 UB 搬运与向量 repeat）
XOF_BYTES = SHAKE128_RATE * XOF_SQUEEZE_BLOCKS  # 672

# 三字节一组 (c0,c1,c2) 解交织为一个 (d1,d2) 候选对
CAND_PAIRS = XOF_BYTES // 3  # 224

# rej 规范扫描顺序：每对产生 2 个 stream lane（d1 后 d2）
STREAM_LEN = CAND_PAIRS * 2  # 448

# d1 或 d2 数组的字节数（int32 元素，各 224 个）
D12_BYTES = CAND_PAIRS * 4
