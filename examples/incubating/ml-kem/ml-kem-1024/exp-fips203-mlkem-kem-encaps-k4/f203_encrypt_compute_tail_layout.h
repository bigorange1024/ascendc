/**
 * @file f203_encrypt_compute_tail_layout.h
 * @brief Alg.14 compute（行 2/16–21）+ tail（行 20/22–24）**统一 GM 契约**（ml_kem_1024 / k=4）。
 *
 * 用途：本目录 host 单 session 分配 device arena 时，compute launch 写、tail launch 读的
 *       **权威偏移**须与此一致。尺寸派生自：
 *         pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/compute/f203_l18_l19_tiling.h
 *         pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/f203_encrypt_tail_layout.h
 *
 * 背景：T17 — SIM **1 launch** 内联 pack；CPU 仍 4 launch。全链 prep 并入为下一步。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024（k=4）K-PKE.Encrypt；本文件属 stable-fips203-mlkem-pke-encrypt-k4。
 * 与 golden：最终对拍 output/c.bin（中间态默认不落盘）。
 */
#ifndef F203_ENCRYPT_COMPUTE_TAIL_LAYOUT_H
#define F203_ENCRYPT_COMPUTE_TAIL_LAYOUT_H

#include <stdint.h>

#define F203_ECT_K 4U
#define F203_ECT_N 256U
#define F203_ECT_Q 3329U

/* ---------- 与 tail 探针一致（行 20/22–24） ---------- */
#define F203_ECT_MSG_BYTES 32U
#define F203_ECT_U_BYTES (F203_ECT_K * F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_V_BYTES (F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_MU_BYTES (F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_C1_POLY_BYTES 352U
#define F203_ECT_C1_BYTES (F203_ECT_K * F203_ECT_C1_POLY_BYTES)
#define F203_ECT_C2_BYTES 160U
#define F203_ECT_C_BYTES (F203_ECT_C1_BYTES + F203_ECT_C2_BYTES)

/* ---------- 与 compute 探针一致（行 2/16–21） ---------- */
#define F203_ECT_EK_PKE_BYTES (F203_ECT_K * 384U)
#define F203_ECT_Y_BYTES (F203_ECT_K * F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_A_HAT_BYTES (F203_ECT_K * F203_ECT_K * F203_ECT_N * (uint32_t)sizeof(int32_t))
#define F203_ECT_E1_BYTES F203_ECT_U_BYTES
#define F203_ECT_E2_BYTES F203_ECT_V_BYTES
#define F203_ECT_Y_HAT_BYTES F203_ECT_Y_BYTES
#define F203_ECT_U_NTT_BYTES F203_ECT_U_BYTES
#define F203_ECT_U_TR_BYTES (5U * F203_ECT_N * (uint32_t)sizeof(int32_t))
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

/** workspace 字节数：与 f203_l18_l19_tiling.h `tiling::wssize` 同值（331776；vendoring 后 static_assert） */
#define F203_ECT_WS_BYTES 331776U

/** Fused trace：f203_encrypt_l18_l19 16 slot × int32 */
#define F203_ECT_TRACE_BYTES (16U * (uint32_t)sizeof(int32_t))

#endif
