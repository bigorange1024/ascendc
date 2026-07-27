// @probe exp-fips203-mlkem-kem-decaps-k2
// @file prep/presample/f203_se_stage_config.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样与 NTT17 链入口；从 seed 派生设备侧中间量供 alg7/alg8/ahat。 / Presample + Keccak/PRF device vector entry. 本文件 `f203_se_stage_config.hpp` 为该子模块组件。 / Component: f203_se_stage_config.hpp.
// @production_io D14 默认 I/O：input/ ek_pke.bin(800B)+m.bin+coins.bin；output/c.bin(768B)；中间 GM 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends prep 子模块头文件 + CANN AscendC；entry 由 f203_keygen_prep_entry 聚合。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。



// @encrypt_probe_note 本文件在 D14 中仅保留 presample 子组件配置；实际生产入口为 f203_encrypt_prep_entry.cpp。
/**
 * @file f203_se_stage_config.hpp
 * @brief 行 8–15 设备预采样阶段开关（由 CMake / run.sh 注入，勿手改本文件）。
 *
 * | 宏 | 语义 | 默认 |
 * |----|------|------|
 * | F203_SE_VECTOR_V3 | G 标量 + shake batch PRF + **标量 CBD**（P1b-single） | **ON（生产 / 集成）** |
 * | F203_SE_VECTOR_V25 | bulk UB CBD（一次 1024B GM→UB） | OFF（实验对照，SIM 更慢，**禁止接入**） |
 *
 * 历史名 F203_SE_VECTOR_V4 已废除，仅作编译期别名 → V25（见下）。
 */
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
