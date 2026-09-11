/**
 * @file tiling.h
 * @brief RB-T10：u,v 设备 MIX 真拓扑 — Host 预喂 Â/ŷ/t̂/e/μ，设备算 u,v。
 *
 * Flag 表（对齐 T03；永禁 5/7）：
 *   1 = AIV→AIC 就绪（NTT 握手段与 INTT 握手段复用）
 *   3 = AIC→AIV 完成（同上复用）
 *   4 = AIV→AIC GATE
 *
 * 时序：
 *   AIC: Wait(1)→CubeNTT→Set(3) → Wait(4) → Wait(1)→CubeINTT→Set(3)
 *   AIV: Set(1)→Wait(3) → MultiplyNTTs(Âᵀ∘ŷ,⟨t̂,ŷ⟩) → Set(4)
 *        → Set(1)→Wait(3) → INTT+e/μ → 写 u,v
 *
 * 背景：T08=Host 孪生；T09=外形双 launch 且 Host 预喂 u,v；本刀关闭 G3 设备侧。
 * 结论：主验收 u,v 对拍 + 不挂；禁抄 alg14/encrypt/frozen。
 */
#ifndef RB_T10_UV_DEVICE_MIX_TILING_H
#define RB_T10_UV_DEVICE_MIX_TILING_H

#include <cstddef>
#include <cstdint>

/** Host→Device 运行时参数（固定 64B 槽，当前仅占位）。 */
struct TilingData {
    int32_t reserved0;
    int32_t reserved1;
};

namespace tiling {

/** ML-KEM-1024 拓扑常量（与 T08 LAYOUT / FIPS k=4 一致）。 */
constexpr int32_t kKem = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
/** FIPS 203 Alg.10：128^{-1} mod q。 */
constexpr int32_t kInttScale = 3303;
constexpr int32_t kZetaN = 128;
constexpr int32_t kGammaN = 128;

constexpr size_t kPolyBytes = static_cast<size_t>(kPolyN) * sizeof(int32_t); // 1024
constexpr size_t kAHatBytes = static_cast<size_t>(kKem) * kKem * kPolyBytes; // 16384
constexpr size_t kYHatBytes = static_cast<size_t>(kKem) * kPolyBytes;       // 4096
constexpr size_t kTHatBytes = kYHatBytes;
constexpr size_t kE1Bytes = kYHatBytes;
constexpr size_t kE2Bytes = kPolyBytes;
constexpr size_t kMuBytes = kPolyBytes;
constexpr size_t kZetasBytes = static_cast<size_t>(kZetaN) * sizeof(int32_t);   // 512
constexpr size_t kGammasBytes = static_cast<size_t>(kGammaN) * sizeof(int32_t); // 512
constexpr size_t kUHatBytes = kYHatBytes;
constexpr size_t kVHatBytes = kPolyBytes;
constexpr size_t kUBytes = kYHatBytes;
constexpr size_t kVBytes = kPolyBytes;
constexpr size_t kUvOutBytes = kUBytes + kVBytes; // 5120：独立落盘缓冲 u‖v

/** 极轻 Cube：C[16,32] int32 = A[16,32] int8 @ B[32,32] int8（握手段有界真算）。 */
constexpr uint16_t kM = 16;
constexpr uint16_t kKk = 32; /**< Cube 内积维；勿与 kKem 混淆 */
constexpr uint16_t kN = 32;
constexpr size_t kMatABytes = static_cast<size_t>(kM) * kKk;
constexpr size_t kMatBBytes = static_cast<size_t>(kKk) * kN;
constexpr size_t kMatCBytes = static_cast<size_t>(kM) * kN * sizeof(int32_t);
constexpr size_t kOutBytes = 64;

constexpr size_t kTraceSlots = 20;
constexpr size_t kTraceBytes = kTraceSlots * sizeof(uint32_t);

/**
 * 共享 workspace（Host 预装输入；设备写 û/v̂、u/v、mat_c、TRACE）：
 *   A_HAT…MU / ZETAS / GAMMAS → Host H2D
 *   U_HAT / V_HAT → AIV0 MultiplyNTTs 段写出
 *   U / V → AIV0 INTT+加噪写出（亦镜像到独立 uvOut）
 *   MAT_* → 极轻 Cube
 */
constexpr size_t OFF_A_HAT = 0;
constexpr size_t OFF_Y_HAT = OFF_A_HAT + kAHatBytes;
constexpr size_t OFF_T_HAT = OFF_Y_HAT + kYHatBytes;
constexpr size_t OFF_E1 = OFF_T_HAT + kTHatBytes;
constexpr size_t OFF_E2 = OFF_E1 + kE1Bytes;
constexpr size_t OFF_MU = OFF_E2 + kE2Bytes;
constexpr size_t OFF_ZETAS = OFF_MU + kMuBytes;
constexpr size_t OFF_GAMMAS = OFF_ZETAS + kZetasBytes;
constexpr size_t OFF_U_HAT = OFF_GAMMAS + kGammasBytes;
constexpr size_t OFF_V_HAT = OFF_U_HAT + kUHatBytes;
constexpr size_t OFF_U = OFF_V_HAT + kVHatBytes;
constexpr size_t OFF_V = OFF_U + kUBytes;
constexpr size_t OFF_MAT_A = OFF_V + kVBytes;
constexpr size_t OFF_MAT_B = OFF_MAT_A + kMatABytes;
constexpr size_t OFF_MAT_C_NTT = OFF_MAT_B + kMatBBytes;
constexpr size_t OFF_MAT_C_INTT = OFF_MAT_C_NTT + kMatCBytes;
constexpr size_t OFF_TRACE = OFF_MAT_C_INTT + kMatCBytes;
constexpr size_t wssize = OFF_TRACE + kTraceBytes;

/** CrossCore flagId：1/3 复用；4=GATE；永禁 5/7。 */
constexpr uint16_t kFlagAivReady = 1;
constexpr uint16_t kFlagAicDone = 3;
constexpr uint16_t kFlagGate = 4;

/** TRACE 槽（与 T03 同编号，便于 sync 对读）。 */
constexpr uint32_t SLOT_HOST_PRE = 0;
constexpr uint32_t SLOT_AIV0_PRE_SET1_NTT = 1;
constexpr uint32_t SLOT_AIV1_PRE_SET1_NTT = 2;
constexpr uint32_t SLOT_AIC_POST_WAIT1_NTT = 3;
constexpr uint32_t SLOT_AIC_PRE_SET3_NTT = 4;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_NTT = 5;
constexpr uint32_t SLOT_AIV0_PRE_SET4 = 6;
constexpr uint32_t SLOT_AIC_POST_WAIT4 = 7;
constexpr uint32_t SLOT_HOST_POST_SYNC = 8;
constexpr uint32_t SLOT_AIV0_PRE_SET1_INTT = 9;
constexpr uint32_t SLOT_AIC_POST_WAIT1_INTT = 10;
constexpr uint32_t SLOT_AIC_PRE_SET3_INTT = 11;
constexpr uint32_t SLOT_AIV0_POST_WAIT3_INTT = 12;
constexpr uint32_t SLOT_AIV1_POST_WAIT3_INTT = 13;
constexpr uint32_t SLOT_AIV1_PRE_SET4 = 14;
constexpr uint32_t SLOT_AIV1_PRE_SET1_INTT = 15;
constexpr uint32_t SLOT_AIV1_POST_WAIT3_NTT = 16;
/** AIV0：MultiplyNTTs 段完成（软：仅诊断）。 */
constexpr uint32_t SLOT_AIV0_MUL_DONE = 17;
/** AIV0：INTT+加噪写完 u,v（软）。 */
constexpr uint32_t SLOT_AIV0_UV_DONE = 18;

constexpr uint32_t MAGIC_HOST_PRE = 0x484F5354u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_NTT = 0xA1010001u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_NTT = 0xA1110001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_NTT = 0xC1010001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_NTT = 0xC1030003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_NTT = 0xA1030003u;
constexpr uint32_t MAGIC_AIV0_PRE_SET4 = 0xA1040004u;
constexpr uint32_t MAGIC_AIC_POST_WAIT4 = 0xC1040004u;
constexpr uint32_t MAGIC_HOST_POST_SYNC = 0x484F5355u;
constexpr uint32_t MAGIC_AIV0_PRE_SET1_INTT = 0xA1011001u;
constexpr uint32_t MAGIC_AIC_POST_WAIT1_INTT = 0xC1011001u;
constexpr uint32_t MAGIC_AIC_PRE_SET3_INTT = 0xC1031003u;
constexpr uint32_t MAGIC_AIV0_POST_WAIT3_INTT = 0xA1031003u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_INTT = 0xA1131003u;
constexpr uint32_t MAGIC_AIV1_PRE_SET4 = 0xA1140004u;
constexpr uint32_t MAGIC_AIV1_PRE_SET1_INTT = 0xA1111001u;
constexpr uint32_t MAGIC_AIV1_POST_WAIT3_NTT = 0xA1130003u;
constexpr uint32_t MAGIC_AIV0_MUL_DONE = 0x4D554C44u; // "MULD"
constexpr uint32_t MAGIC_AIV0_UV_DONE = 0x5556444Eu;  // "UVDN"
constexpr uint32_t MAGIC_OUT_OK = 0x543A0033u;        // T10

} // namespace tiling

#endif
