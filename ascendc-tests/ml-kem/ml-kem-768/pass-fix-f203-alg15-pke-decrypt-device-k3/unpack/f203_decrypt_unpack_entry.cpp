/**
 * @file f203_decrypt_unpack_entry.cpp
 * @brief Alg.15 行 3–4 独立 kernel：c → (u', v')（ByteDecode + Decompress）。
 *
 * 流水线位置：历史 G1；生产 fused 用 unpack_impl.hpp（Decode 标量 + Decompress 向量）。
 * d_u=10（c₁ 每 poly 320B），d_v=4（c₂ 128B）。
 * 与 golden：gate_g1 golden_u / golden_v。
 */
#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

using namespace AscendC;

namespace {

constexpr int32_t kN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kQ = static_cast<int32_t>(F203_DECRYPT_Q);

/** Decompress₁₀：round(u·q/2¹⁰)。 */
__aicore__ inline uint32_t decompress_d10_u32(uint32_t u)
{
    return ((u * static_cast<uint32_t>(kQ)) + 512u) >> 10;
}

/** Decompress₄：round(u·q/2⁴)。 */
__aicore__ inline uint32_t decompress_d4_u32(uint32_t u)
{
    return ((u * static_cast<uint32_t>(kQ)) + 8u) >> 4;
}

/** 从 packed bits 解出 256 个 d-bit 整数（Alg.6 逆）。 */
__aicore__ inline void byte_decode_bits_scalar(int32_t *out, const uint8_t *in, uint32_t dBits)
{
    uint32_t bitPos = 0U;
    const uint32_t mask = (dBits >= 32U) ? 0xFFFFFFFFu : ((1U << dBits) - 1U);
    for (uint32_t i = 0; i < static_cast<uint32_t>(kN); ++i) {
        uint32_t a = 0U;
        for (uint32_t j = 0; j < dBits; ++j) {
            const uint32_t byteIdx = bitPos >> 3;
            const uint32_t bitIdx = bitPos & 7U;
            if ((in[byteIdx] >> bitIdx) & 1U) {
                a |= (1U << j);
            }
            ++bitPos;
        }
        out[i] = static_cast<int32_t>(a & mask);
    }
}

/** 单 poly：ByteDecode₁₀ + Decompress₁₀ → Z_q 系数。 */
__aicore__ inline void unpack_poly_u10(int32_t *polyOut, const uint8_t *cPoly)
{
    int32_t comp[kN];
    byte_decode_bits_scalar(comp, cPoly, 10U);
    for (int32_t i = 0; i < kN; ++i) {
        polyOut[i] = static_cast<int32_t>(decompress_d10_u32(static_cast<uint32_t>(comp[i])));
    }
}

/** 单 poly：ByteDecode₄ + Decompress₄ → Z_q 系数。 */
__aicore__ inline void unpack_poly_v4(int32_t *polyOut, const uint8_t *cPoly)
{
    int32_t comp[kN];
    byte_decode_bits_scalar(comp, cPoly, 4U);
    for (int32_t i = 0; i < kN; ++i) {
        polyOut[i] = static_cast<int32_t>(decompress_d4_u32(static_cast<uint32_t>(comp[i])));
    }
}

} // namespace

/**
 * unpack 独立入口。
 * @param cGm 密文 c₁‖c₂；uGm/vGm 输出 int32 平面
 * 前置：非 AIC；仅 block0。
 */
extern "C" __global__ __aicore__ void f203_decrypt_unpack_c(GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm)
{
#if defined(ASCENDC_CPU_DEBUG)
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
#else
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    if (AscendC::GetSubBlockNum() == 1) {
        return;
    }
#endif
    if (GetBlockIdx() != 0) {
        return;
    }

    const auto *cIn = reinterpret_cast<const __gm__ uint8_t *>(cGm);
    auto *uOut = reinterpret_cast<__gm__ int32_t *>(uGm);
    auto *vOut = reinterpret_cast<__gm__ int32_t *>(vGm);

    int32_t uLocal[kK * kN];
    int32_t vLocal[kN];
    // ---- c₁：k 个 d=10 poly → u' ----
    for (int32_t p = 0; p < kK; ++p) {
        uint8_t cPolyLocal[F203_C1_POLY_BYTES];
        const uint32_t cOff = static_cast<uint32_t>(p) * F203_C1_POLY_BYTES;
        for (uint32_t b = 0; b < F203_C1_POLY_BYTES; ++b) {
            cPolyLocal[b] = cIn[cOff + b];
        }
        unpack_poly_u10(uLocal + p * kN, cPolyLocal);
    }
    // ---- c₂：d=4 → v' ----
    uint8_t c2Local[F203_C2_BYTES];
    for (uint32_t b = 0; b < F203_C2_BYTES; ++b) {
        c2Local[b] = cIn[F203_C1_BYTES + b];
    }
    unpack_poly_v4(vLocal, c2Local);

    // 写回 GM（独立 launch 可用标量写）
    for (int32_t i = 0; i < kK * kN; ++i) {
        uOut[i] = uLocal[i];
    }
    for (int32_t i = 0; i < kN; ++i) {
        vOut[i] = vLocal[i];
    }
}

#ifndef __CCE_KT_TEST__
/** Host launch 包装。 */
void f203_decrypt_unpack_c_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *cGm, uint8_t *uGm,
                              uint8_t *vGm)
{
    f203_decrypt_unpack_c<<<blockDim, l2ctrl, stream>>>(cGm, uGm, vGm);
}
#endif
