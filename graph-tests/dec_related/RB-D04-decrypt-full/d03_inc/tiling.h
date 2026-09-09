/**
 * @file tiling.h
 * @brief RB-D03：Decrypt L2b — INTT(ŵ)+extract → m[32]。
 *
 * Flag 表（S0A / KB §B2；与 L2a 分核复用号，靠 host sync 隔离）：
 *   1 = AIV→AIC 就绪
 *   3 = AIC→AIV Cube 完成
 *   4 = GATE（可选收口；本刀启用）
 * 永禁 5 / 7；AIC Wait 环内禁 SyncAll。
 *
 * 时序（本核内，勿与 D02 同 launch）：
 *   AIV: Set(4) → Set(1) → Wait(3) → w←INTT(ŵ) → m←ByteEncode₁(Compress₁(v−w))
 *   AIC: Wait(4) → Wait(1) → Cube → Set(3)
 *
 * 输入布局对齐：
 *   - ŵ：D02 写出单 poly int32[256]（无 Tag5T pad；本刀标量 Alg.10）
 *   - v：D01 写出单 poly int32[256]（Decompress₅ 后）
 * 输出：m uint8[32]（Alg.15 尾段）。
 */
#ifndef RB_D03_DECRYPT_INTT_EXTRACT_TILING_H
#define RB_D03_DECRYPT_INTT_EXTRACT_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024：N=256；本刀单 poly（ŵ/v）。 */
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr int32_t kZetaN = 128;
/** FIPS 203 Alg.10：乘以 128^{−1} mod q = 3303。 */
constexpr int32_t kInttScale = 3303;
/** Compress_d 统一乘数 C=⌊2^{37}/q⌋（d=1：shift=36，bias=2^{35}）。 */
constexpr int64_t kCompressC = 41285357LL;
constexpr int32_t kCompress1Shift = 36;
constexpr int64_t kCompress1Bias = 1LL << 35;

constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t); // 1024
constexpr size_t kWHatBytes = kPolyBytes;
constexpr size_t kVBytes = kPolyBytes;
constexpr size_t kWBytes = kPolyBytes; // 可选中间 w（设备写出，便于分段对拍）
constexpr size_t kMBytes = 32;
constexpr size_t kZetasBytes = static_cast<size_t>(kZetaN) * sizeof(int32_t);

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kKk = 32;
constexpr uint16_t kN = 32;
constexpr size_t kMatABytes = static_cast<size_t>(kM) * kKk;
constexpr size_t kMatBBytes = static_cast<size_t>(kKk) * kN;
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t);
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 14;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/**
 * 共享 workspace 布局（字节偏移）：
 *   W_HAT / V / ZETAS → Host H2D（契约对齐 D02 ŵ、D01 v）
 *   W → AIV0 中间时域（可选落盘）
 *   MAT_* → 极轻 Cube
 *   TRACE → 握手诊断
 */
constexpr size_t OFF_W_HAT = 0;
constexpr size_t OFF_V = OFF_W_HAT + kWHatBytes;
constexpr size_t OFF_ZETAS = OFF_V + kVBytes;
constexpr size_t OFF_W = OFF_ZETAS + kZetasBytes;
constexpr size_t OFF_MAT_A = OFF_W + kWBytes;
constexpr size_t OFF_MAT_B = OFF_MAT_A + kMatABytes;
constexpr size_t OFF_MAT_C = OFF_MAT_B + kMatBBytes;
constexpr size_t OFF_TRACE = OFF_MAT_C + kMatCBytes;
constexpr size_t wssize = OFF_TRACE + kTraceBytes;

/** CrossCore flagId：1/3 + GATE(4)；永禁 5/7。 */
constexpr uint16_t kFlagGate = 4;
constexpr uint16_t kFlagAivReady = 1;
constexpr uint16_t kFlagAicDone = 3;

/** TRACE 槽。 */
constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_AIV0_PRE_SET4 = 1;
constexpr uint32_t SLOT_AIV1_PRE_SET4 = 2;
constexpr uint32_t SLOT_AIV0_PRE_SET1 = 3;
constexpr uint32_t SLOT_AIV1_PRE_SET1 = 4;
constexpr uint32_t SLOT_AIC_POST_WAIT4 = 5;
constexpr uint32_t SLOT_AIC_POST_WAIT1 = 6;
constexpr uint32_t SLOT_AIC_PRE_SET3 = 7;
constexpr uint32_t SLOT_AIV0_POST_WAIT3 = 8;
constexpr uint32_t SLOT_AIV1_POST_WAIT3 = 9;
constexpr uint32_t SLOT_AIV0_DONE = 10;
constexpr uint32_t SLOT_HOST_POST_SYNC = 11;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_AIV0_PRE_SET4 = 0xA1040004u;
constexpr uint32_t MAGIC_AIV1_PRE_SET4 = 0xA1140004u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1 = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1 = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT4 = 0xC1040004u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1 = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3 = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3 = 0xA1030003u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3 = 0xA1130003u;
constexpr uint32_t MAGIC_AIV0_DONE = 0x4430334Fu; // "D03O"
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_OUT_OK = 0x4430333Au; // "D03:"

} // namespace tiling

#endif
