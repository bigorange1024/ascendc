/**
 * @file f203_se_stage_config.hpp
 * @brief SE 向量采样阶段编译配置（PRF/CBD/SHAKE）。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024 Encrypt prep。
 * 默认生产全量路径。
 */

// @probe stable-fips203-mlkem-pke-keygen-k4
// @file prep/presample/f203_se_stage_config.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样与 NTT17 链入口；从 seed 派生设备侧中间量供 alg7/alg8/ahat。 / Presample + Keccak/PRF device vector entry. 本文件 `f203_se_stage_config.hpp` 为该子模块组件。 / Component: f203_se_stage_config.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends prep 子模块头文件 + CANN AscendC；entry 由 f203_keygen_prep_entry 聚合。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

#pragma once

#if defined(F203_SE_VECTOR_V4) && !defined(F203_SE_VECTOR_V25)
#define F203_SE_VECTOR_V25 1
#pragma message("F203_SE_VECTOR_V4 is obsolete; use F203_SE_VECTOR_V25 (experimental, not for integration)")
#endif

#if defined(F203_SE_VECTOR_V25) && defined(F203_SE_VECTOR_V3)
#error "F203_SE_VECTOR_V3 and F203_SE_VECTOR_V25 are mutually exclusive"
#endif

#if !defined(F203_SE_VECTOR_V25) && !defined(F203_SE_VECTOR_V3)
#define F203_SE_VECTOR_V3 1
#endif

#if defined(F203_SE_VECTOR_V25)
#define F203_SE_STAGE_LABEL "V2.5-experimental"
#elif defined(F203_SE_VECTOR_V3)
#define F203_SE_STAGE_LABEL "V3-production"
#else
#define F203_SE_STAGE_LABEL "unknown"
#endif
