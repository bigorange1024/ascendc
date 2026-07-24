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
