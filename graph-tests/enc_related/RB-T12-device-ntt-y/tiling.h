/**
 * @file tiling.h
 * @brief RB-T12：设备 ŷ←NTT(y) — Host 预喂 y/ζ，MIX AIV0 正向 NTT。
 *
 * Flag 表（对齐 T03 纪律；本刀仅 NTT 握手段）：
 *   1 = AIV→AIC 就绪
 *   3 = AIC→AIV Cube 完成
 * 永禁 5 / 7；本刀无 GATE(4)。
 *
 * 时序：
 *   AIC: Wait(1) → CubeNTT → Set(3)
 *   AIV: Set(1) → Wait(3) → Alg.9 NTT(y)→ŷ（AIV0；poly-batch 整 poly）
 *
 * 背景：接 T07 prep 的 y；T10 已有设备 INTT 标量形态；本刀关 Encrypt 行 16。
 * 结论：主验收 ŷ 对拍 + 不挂；禁抄 alg14/encrypt/frozen；S1–S3 禁 Gather（本刀不走 Gather）。
 */
#ifndef RB_T12_DEVICE_NTT_Y_TILING_H
#define RB_T12_DEVICE_NTT_Y_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024：k=4 polyvec；N=256。 */
constexpr int32_t kKem = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr int32_t kZetaN = 128;

constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t); // 1024
constexpr size_t kYBytes = static_cast<size_t>(kKem) * kPolyBytes;           // 4096
constexpr size_t kYHatBytes = kYBytes;
constexpr size_t kZetasBytes = static_cast<size_t>(kZetaN) * sizeof(int32_t); // 512

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（握手段有界真算）。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kKk = 32;
constexpr uint16_t kN = 32;
constexpr size_t kMatABytes = static_cast<size_t>(kM) * kKk;
constexpr size_t kMatBBytes = static_cast<size_t>(kKk) * kN;
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t);
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 12;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/**
 * 共享 workspace：
 *   Y / ZETAS → Host H2D
 *   Y_HAT → AIV0 NTT 写出（亦镜像到独立 yHatOut）
 *   MAT_* → 极轻 Cube
 *   TRACE → 握手诊断
 */
constexpr size_t OFF_Y = 0;
constexpr size_t OFF_ZETAS = OFF_Y + kYBytes;
constexpr size_t OFF_Y_HAT = OFF_ZETAS + kZetasBytes;
constexpr size_t OFF_MAT_A = OFF_Y_HAT + kYHatBytes;
constexpr size_t OFF_MAT_B = OFF_MAT_A + kMatABytes;
constexpr size_t OFF_MAT_C_NTT = OFF_MAT_B + kMatBBytes;
constexpr size_t OFF_TRACE = OFF_MAT_C_NTT + kMatCBytes;
constexpr size_t wssize = OFF_TRACE + kTraceBytes;

/** CrossCore flagId：仅 1/3；永禁 5/7。 */
constexpr uint16_t kFlagAivReady = 1;
constexpr uint16_t kFlagAicDone = 3;

/** TRACE 槽。 */
constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_AIV0_PRE_SET1_NTT = 1;
constexpr uint32_t SLOT_AIV1_PRE_SET1_NTT = 2;
constexpr uint32_t SLOT_AIC_POST_WAIT1_NTT = 3;
constexpr uint32_t SLOT_AIC_PRE_SET3_NTT = 4;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_NTT = 5;
constexpr uint32_t SLOT_HOST_POST_SYNC = 6;
constexpr uint32_t SLOT_AIV1_POST_WAIT3_NTT = 7;
/** AIV0：ŷ 写完（软）。 */
constexpr uint32_t SLOT_AIV0_YHAT_DONE = 8;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_NTT = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_NTT = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_NTT = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_NTT = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_NTT = 0xA1030003u;
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_NTT = 0xA1130003u;
constexpr uint32_t MAGIC_AIV0_YHAT_DONE = 0x59484154u; // "YHAT"
constexpr uint32_t MAGIC_OUT_OK = 0x543A003Cu;         // T12

} // namespace tiling

#endif
