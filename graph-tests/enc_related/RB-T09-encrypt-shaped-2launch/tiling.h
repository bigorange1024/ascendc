/**
 * @file tiling.h
 * @brief RB-T09：Encrypt 外形双 launch — Launch1 prep（AIV）+ Launch2 compute MIX。
 *
 * Launch2 Flag 表（对齐 T03 纪律；永禁 5/7）：
 *   1 = AIV→AIC 就绪（NTT 段与 INTT 段复用）
 *   3 = AIC→AIV 完成（同上复用）
 *   4 = AIV→AIC GATE
 *
 * 时序（compute）：
 *   AIC: Wait(1)→CubeNTT→Set(3) → Wait(4) → Wait(1)→CubeINTT→Set(3)
 *   AIV: Set(1)→Wait(3) → Set(4) → Set(1)→Wait(3) →（半桩）pack u,v→c
 *
 * 背景：T08 无设备核，本刀自建 MIX；T07 产出语义由 Host 预算后经 Launch1 落盘。
 * 结论：主验收 SIM 双 launch 不挂；PASS_SYNC 与 PASS_IO 分栏。
 */
#ifndef RB_T09_ENCRYPT_SHAPED_2LAUNCH_TILING_H
#define RB_T09_ENCRYPT_SHAPED_2LAUNCH_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024 外形常量（与 T06/T07 LAYOUT 一致）。 */
constexpr int32_t kK = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr size_t kEkBytes = 1568;
constexpr size_t kCoinsBytes = 32;
constexpr size_t kRhoBytes = 32;
constexpr size_t kYe1e2Bytes = 9 * kPolyN * sizeof(int32_t); // 9216
constexpr size_t kUBytes = static_cast<size_t>(kK) * kPolyN * sizeof(int32_t); // 4096
constexpr size_t kVBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t);      // 1024
constexpr size_t kCBytes = 1568;

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kKk = 32;
constexpr uint16_t kN = 32;
constexpr size_t kMatABytes = static_cast<size_t>(kM) * kKk;
constexpr size_t kMatBBytes = static_cast<size_t>(kKk) * kN;
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t);
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 24;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/**
 * 共享 workspace 布局（两 launch 共用同一 GM 缓冲）：
 *   Y_E1_E2 / RHO / PREP_MARK → Launch1 写
 *   MAT_* / U / V / TRACE → Host 预装 + Launch2 写 mat_c / TRACE
 *   C 由 Launch2 AIV pack 写出（亦有独立 cOut 指针，与 ws 内 C 同内容）
 */
constexpr size_t OFF_Y_E1_E2 = 0;
constexpr size_t OFF_RHO = OFF_Y_E1_E2 + kYe1e2Bytes;
constexpr size_t OFF_PREP_MARK = OFF_RHO + kRhoBytes;
constexpr size_t OFF_MAT_A = OFF_PREP_MARK + 64;
constexpr size_t OFF_MAT_B = OFF_MAT_A + kMatABytes;
constexpr size_t OFF_MAT_C_NTT = OFF_MAT_B + kMatBBytes;
constexpr size_t OFF_MAT_C_INTT = OFF_MAT_C_NTT + kMatCBytes;
constexpr size_t OFF_U = OFF_MAT_C_INTT + kMatCBytes;
constexpr size_t OFF_V = OFF_U + kUBytes;
constexpr size_t OFF_C = OFF_V + kVBytes;
constexpr size_t OFF_TRACE = OFF_C + kCBytes;
constexpr size_t wssize = OFF_TRACE + kTraceBytes;

/** CrossCore flagId：1/3 复用；4=GATE；永禁 5/7。 */
constexpr uint16_t kFlagAivReady = 1;
constexpr uint16_t kFlagAicDone = 3;
constexpr uint16_t kFlagGate = 4;

/** TRACE 槽。 */
constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_PREP_DONE = 1;
constexpr uint32_t SLOT_AIV0_PRE_SET1_NTT = 2;
constexpr uint32_t SLOT_AIV1_PRE_SET1_NTT = 3;
constexpr uint32_t SLOT_AIC_POST_WAIT1_NTT = 4;
constexpr uint32_t SLOT_AIC_PRE_SET3_NTT = 5;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_NTT = 6;
constexpr uint32_t SLOT_AIV0_PRE_SET4 = 7;
constexpr uint32_t SLOT_AIC_POST_WAIT4 = 8;
constexpr uint32_t SLOT_HOST_MID_SYNC = 9;
constexpr uint32_t SLOT_AIV0_PRE_SET1_INTT = 10;
constexpr uint32_t SLOT_AIC_POST_WAIT1_INTT = 11;
constexpr uint32_t SLOT_AIC_PRE_SET3_INTT = 12;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_INTT = 13;
constexpr uint32_t SLOT_AIV1_POST_WAIT3_INTT = 14;
constexpr uint32_t SLOT_AIV0_PACK_DONE = 15;
constexpr uint32_t SLOT_HOST_POST_SYNC = 16;
constexpr uint32_t SLOT_AIV1_PRE_SET4 = 17;
constexpr uint32_t SLOT_AIV1_PRE_SET1_INTT = 18;

/** 魔数。 */
constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_PREP_DONE = 0x50524550u; // "PREP"
constexpr uint32_t MAGIC_HOST_MID = 0x484F4D49u;  // "HOMI"
constexpr uint32_t MAGIC_HOST_POST = 0x484F5355u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_NTT = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_NTT = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_NTT = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_NTT = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_NTT = 0xA1030003u;
constexpr uint32_t MAGIC_AIV0_PRE_SET4 = 0xA1040004u;
constexpr uint32_t MAGIC_AIC_POST_WAIT4 = 0xC1040004u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_INTT = 0xA1011001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_INTT = 0xC1011001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_INTT = 0xC1031003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_INTT = 0xA1031003u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_INTT = 0xA1131003u;
constexpr uint32_t MAGIC_AIV1_PRE_SET4 = 0xA1140004u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_INTT = 0xA1111001u;
constexpr uint32_t MAGIC_AIV0_PACK_DONE = 0x5041434Bu; // "PACK"
constexpr uint32_t MAGIC_OUT_OK = 0x54390033u;         // T09
constexpr uint32_t MAGIC_PREP_MARK = 0x54395052u;      // T09P

} // namespace tiling

#endif
