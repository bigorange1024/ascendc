/** K04 拼装：自 RB-K0x 契约头复制，仅改 include 路径；算法未改。 */
/**
 * @file tiling.h
 * @brief RB-K02：KeyGen L2a — NTT(ŝ)、NTT(ê)。
 *
 * Flag 表（KB §B2 · MIX · 1/3(+4)）：
 *   1 = AIV→AIC 就绪
 *   3 = AIC→AIV Cube 完成
 *   4 = GATE（本刀启用）
 * 永禁 5 / 7；AIC Wait 环内禁 SyncAll。
 *
 * 时序（本核内）：
 *   AIV: Set(4) → Set(1) → Wait(3) → NTT(ŝ)→NTT(ŝ)；NTT(ê)→NTT(ê) [AIV0]
 *   AIC: Wait(4) → Wait(1) → Cube → Set(3)
 *
 * 背景：接 K01 mid-sync 后的 ŝ/ê[4,256] int32；本刀 gen_data 可自洽造同 SEED_D。
 * 结论：主验收 NTT(ŝ)/NTT(ê) 对拍；poly-batch 整 poly；禁 Gather / limbsplit。
 */
#ifndef RB_K02_KG_NTT_TILING_H
#define RB_K02_KG_NTT_TILING_H

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
constexpr size_t kPolyvecBytes = static_cast<size_t>(kKem) * kPolyBytes;     // 4096
constexpr size_t kSBytes = kPolyvecBytes;
constexpr size_t kEBytes = kPolyvecBytes;
constexpr size_t kSNttBytes = kPolyvecBytes;
constexpr size_t kENttBytes = kPolyvecBytes;
constexpr size_t kZetasBytes = static_cast<size_t>(kZetaN) * sizeof(int32_t); // 512

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（握手段有界真算）。 */
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
 *   S / E / ZETAS → Host H2D（时域 ŝ/ê + ζ）
 *   S_NTT / E_NTT → AIV0 写出（亦镜像独立 out 缓冲）
 *   MAT_* → 极轻 Cube
 *   TRACE → 握手诊断
 */
constexpr size_t OFF_S = 0;
constexpr size_t OFF_E = OFF_S + kSBytes;
constexpr size_t OFF_ZETAS = OFF_E + kEBytes;
constexpr size_t OFF_S_NTT = OFF_ZETAS + kZetasBytes;
constexpr size_t OFF_E_NTT = OFF_S_NTT + kSNttBytes;
constexpr size_t OFF_MAT_A = OFF_E_NTT + kENttBytes;
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
constexpr uint32_t MAGIC_AIV0_DONE = 0x4B30324Fu; // "K02O"
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_OUT_OK = 0x4B30323Au; // "K02:"

} // namespace tiling

#endif
