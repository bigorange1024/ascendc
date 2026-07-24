/**
 * @file f203_a_hat16_config.h
 * @brief 16-poly Â 探针编译开关（与单 poly alg7 config 对齐）。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024（k=4）K-PKE.Encrypt；本文件属 stable-fips203-mlkem-pke-encrypt-k4。
 * 与 golden：中间态不落盘时最终对拍 output/c.bin；本文件职责见上文 @brief。
 */
// @probe stable-fips203-mlkem-pke-keygen-k4
// @file prep/ahat/f203_a_hat16_config.h
// @layer prep
// @role prep/ahat：设备侧生成矩阵 A_hat（FIPS203 Alg.6/布局 f203_a_hat16）；AIV-only UB 流水，为 compute MMAD 提供 a_hat GM。 / Device A_hat generation for keygen prep. 本文件 `f203_a_hat16_config.h` 为该子模块组件。 / Component: f203_a_hat16_config.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends prep 子模块头文件 + CANN AscendC；entry 由 f203_keygen_prep_entry 聚合。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

#pragma once

#ifndef F203_ALG7_REJ_IMPL
#define F203_ALG7_REJ_IMPL 1
#endif

#ifndef F203_ALG7_D12_GATHER
#define F203_ALG7_D12_GATHER 0
#endif

/** 1=单 AIV 跑满 16 poly（默认）；2=双 AIV 各 8 poly（polyIdx 0–7 / 8–15）。 */
#ifndef F203_AHAT16_BLOCK_DIM
#define F203_AHAT16_BLOCK_DIM 2
#endif

/**
 * 1=一次 batch16 SHAKE（x 行 64B stride；SIM 可跑但 tick **负优化** ~960k vs 逐条 ~734k）。
 * 0=16×逐条 SHAKE（**默认**，SIM 基线）。
 */
#ifndef F203_AHAT16_BATCH_SHAKE
#define F203_AHAT16_BATCH_SHAKE 0
#endif

/**
 * 1=SHAKE 仅 squeeze 504B（3×rate）；0=672B 默认。
 * 须与 golden ROM 同步（run.sh 导出 F203_ALG7_XOF_504 再 gen ROM / gen_data）。
 */
#ifndef F203_ALG7_XOF_504
#define F203_ALG7_XOF_504 0
#endif
