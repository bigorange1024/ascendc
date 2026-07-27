/**
 * @file f203_encrypt_compute_tail_layout.h
 * @brief Alg.14 compute（行 2/16–21）+ tail（行 20/22–24）**统一 GM 契约**（ML-KEM-512 / k=2）。
 *
 * 用途：本目录 host 单 session 分配 device arena 时，compute launch 写、tail launch 读的
 *       **权威偏移**须与此一致。尺寸派生自：
 *         本目录 compute/f203_l18_l19_tiling.h（由 512 B5/B6 几何收敛）
 *         本目录 f203_encrypt_tail_layout.h（d_u=10, d_v=4，c=768B）
 *
 * 背景：D14 k2 全链采用 prep + compute/tail 两次 launch；CPU 保留分段调试路径。
 */
#ifndef F203_ENCRYPT_COMPUTE_TAIL_LAYOUT_H
#define F203_ENCRYPT_COMPUTE_TAIL_LAYOUT_H

#include <stdint.h>

#define F203_ECT_K 2U
#define F203_ECT_N 256U
#define F203_ECT_Q 3329U

/* ---------- 与 tail 探针一致（行 20/22–24） ---------- */
#define F203_ECT_MSG_BYTES 32U
#define F203_ECT_U_BYTES (F203_ECT_K * F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_V_BYTES (F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_MU_BYTES (F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_C1_POLY_BYTES 320U
#define F203_ECT_C1_BYTES (F203_ECT_K * F203_ECT_C1_POLY_BYTES)
#define F203_ECT_C2_BYTES 128U
#define F203_ECT_C_BYTES (F203_ECT_C1_BYTES + F203_ECT_C2_BYTES)

/* ---------- 与 compute 探针一致（行 2/16–21） ---------- */
#define F203_ECT_EK_PKE_BYTES (F203_ECT_K * 384U + 32U)
#define F203_ECT_Y_BYTES (F203_ECT_K * F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_A_HAT_BYTES (F203_ECT_K * F203_ECT_K * F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_E1_BYTES F203_ECT_U_BYTES
#define F203_ECT_E2_BYTES F203_ECT_V_BYTES
#define F203_ECT_Y_HAT_BYTES F203_ECT_Y_BYTES
#define F203_ECT_U_NTT_BYTES F203_ECT_U_BYTES
#define F203_ECT_U_TR_BYTES (4U * F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_T_HAT_BYTES F203_ECT_U_BYTES
#define F203_ECT_TR_HAT_NTT_BYTES F203_ECT_V_BYTES

/**
 * @brief 逻辑 GM 区域 ID（host 可按 region 独立 aclrtMalloc，或单 arena + 下列 offset）。
 *
 * **拼接红线**：`F203_ECT_REGION_U` / `F203_ECT_REGION_V` 为 compute→tail **唯一 handoff**；
 * tail launch 的 uGm/vGm 指针须与 compute 的 uOut/vOut **同址**（禁止 D2H→H2D 中转）。
 */
typedef enum {
    F203_ECT_REGION_M = 0,
    F203_ECT_REGION_EK_PKE,
    F203_ECT_REGION_Y,
    F203_ECT_REGION_A_HAT,
    F203_ECT_REGION_E1,
    F203_ECT_REGION_E2,
    F203_ECT_REGION_WS,
    /** compute 写 / tail 读 — handoff */
    F203_ECT_REGION_U,
    F203_ECT_REGION_V,
    /** compute 调试落盘（Phase A 可选 D2H） */
    F203_ECT_REGION_Y_HAT,
    F203_ECT_REGION_U_NTT,
    F203_ECT_REGION_U_TR,
    F203_ECT_REGION_T_HAT,
    F203_ECT_REGION_TR_HAT_NTT,
    /** tail 写 */
    F203_ECT_REGION_MU_EMBED,
    F203_ECT_REGION_C,
    F203_ECT_REGION_TRACE,
    F203_ECT_REGION_COUNT
} F203EctRegionId;

/** workspace 字节数：与 f203_l18_l19_tiling.h `tiling::wssize` 同值（按 k2 polyvec4 INTT 上界派生）。 */
#define F203_ECT_WS_BYTES 296960U /* 与 tiling::wssize 同步；polyvec4 INTT engine 保持该上界 */

/** Fused trace：f203_encrypt_l18_l19 16 slot × int32 */
#define F203_ECT_TRACE_BYTES (16U * (uint32_t)sizeof(int32_t))

#endif
