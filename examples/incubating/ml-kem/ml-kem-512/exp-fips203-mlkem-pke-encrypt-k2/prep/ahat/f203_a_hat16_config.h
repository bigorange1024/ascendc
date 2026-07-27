// @probe exp-fips203-mlkem-pke-encrypt-k2
// @file prep/ahat/f203_a_hat16_config.h
// @layer prep
// @role prep/ahat：设备侧生成 Encrypt 用矩阵 A_hat（FIPS203 Alg.14 行 3–7）；AIV-only UB 流水，为 compute MMAD 提供 a_hat GM。
// @production_io 默认 run.sh 生产 I/O：input/ek_pke.bin(800B)+m.bin+coins.bin；output/c.bin(768B)；a_hat/re 仅为 device arena 中间态。
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends prep 子模块头文件 + CANN AscendC；entry 由 f203_encrypt_prep_entry 聚合。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 Â[4,256] 分片构建。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-512（k=2）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/ahat/f203_a_hat16_config.h
 */
/**
 * @file f203_a_hat16_config.h
 * @brief k2 4-poly Â 探针编译开关（与单 poly alg7 config 对齐）。
 */
#pragma once

#ifndef F203_ALG7_REJ_IMPL
#define F203_ALG7_REJ_IMPL 1
#endif

#ifndef F203_ALG7_D12_GATHER
#define F203_ALG7_D12_GATHER 0
#endif

/** 1=单 AIV 跑满 4 poly；2=双 AIV 按 2+2 分片（D14 锁定）。 */
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
