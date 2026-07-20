/**
 * @file f203_decrypt_unpack_entry.cpp
 * @brief Decrypt 流水线（1-kernel fused）G1 独立入口：c GM → u polyvec + v poly。
 *
 * 对齐 FIPS 203 Alg.15 行 3–4：
 *   c₁ → ByteDecode₁₁ + Decompress₁₁ → u'（k 个 poly）
 *   c₂ → ByteDecode₅  + Decompress₅  → v'（1 个 poly）
 * d_u=11 / d_v=5（ml_kem_1024）。
 *
 * 本文件为**分段探针**独立 kernel（全标量 Decode+Decompress）；生产融合路径走
 * `decrypt_g4::unpack_c_impl`（Decode 标量 + Decompress 向量 + DataCopy 落盘）。
 *
 * golden I/O：输入 `input/c.bin`（1568B = 1408+160）；本段输出中间态 u'/v'（生产不落盘）。
 */
#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

using namespace AscendC;

namespace {

constexpr int32_t kN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kQ = static_cast<int32_t>(F203_DECRYPT_Q);

/**
 * Decompress₁₁：u ∈ [0,2¹¹) → ⌊(u·q + 2¹⁰)/2¹¹⌋。
 * 偏置 1024 = 2^(d-1)，右移 11 = d。
 */
__aicore__ inline uint32_t decompress_d11_u32(uint32_t u)
{
    return ((u * static_cast<uint32_t>(kQ)) + 1024u) >> 11;
}

/**
 * Decompress₅：u ∈ [0,2⁵) → ⌊(u·q + 2⁴)/2⁵⌋。
 * 偏置 16 = 2^(d-1)，右移 5 = d。
 */
__aicore__ inline uint32_t decompress_d5_u32(uint32_t u)
{
    return ((u * static_cast<uint32_t>(kQ)) + 16u) >> 5;
}

/**
 * 从 packed bits 解出 256 个 d-bit 整数（FIPS 203 Alg.6 ByteDecode 逆语义，LSB-first）。
 * @param out   输出 [N]；@param in 编码字节流；@param dBits 每系数位数（11 或 5）
 */
__aicore__ inline void byte_decode_bits_scalar(int32_t *out, const uint8_t *in, uint32_t dBits)
{
    uint32_t bitPos = 0U;
    const uint32_t mask = (dBits >= 32U) ? 0xFFFFFFFFu : ((1U << dBits) - 1U);
    for (uint32_t i = 0; i < static_cast<uint32_t>(kN); ++i) {
        uint32_t a = 0U;
        /* 逐 bit 拼装第 i 个系数 */
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

/**
 * 单 poly：c₁ 片段（352B）→ Decode₁₁ → Decompress₁₁ → 时域系数 [N]。
 */
__aicore__ inline void unpack_poly_u11(int32_t *polyOut, const uint8_t *cPoly)
{
    int32_t comp[kN];
    byte_decode_bits_scalar(comp, cPoly, 11U);
    for (int32_t i = 0; i < kN; ++i) {
        polyOut[i] = static_cast<int32_t>(decompress_d11_u32(static_cast<uint32_t>(comp[i])));
    }
}

/**
 * 单 poly：c₂（160B）→ Decode₅ → Decompress₅ → 时域系数 [N]。
 */
__aicore__ inline void unpack_poly_v5(int32_t *polyOut, const uint8_t *cPoly)
{
    int32_t comp[kN];
    byte_decode_bits_scalar(comp, cPoly, 5U);
    for (int32_t i = 0; i < kN; ++i) {
        polyOut[i] = static_cast<int32_t>(decompress_d5_u32(static_cast<uint32_t>(comp[i])));
    }
}

} // namespace

/**
 * 独立 kernel：c → u' + v'。
 * @param cGm 密文 c₁‖c₂（1568B）；@param uGm u'[k×N] int32；@param vGm v'[N] int32
 * 前置：仅 blockIdx==0；AIC 子块空返回。
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

    /* ---- c₁：k 个 poly，各 352B → u'[p] ---- */
    for (int32_t p = 0; p < kK; ++p) {
        uint8_t cPolyLocal[F203_C1_POLY_BYTES];
        const uint32_t cOff = static_cast<uint32_t>(p) * F203_C1_POLY_BYTES;
        for (uint32_t b = 0; b < F203_C1_POLY_BYTES; ++b) {
            cPolyLocal[b] = cIn[cOff + b];
        }
        unpack_poly_u11(uLocal + p * kN, cPolyLocal);
    }

    /* ---- c₂：紧接 c₁ 之后 160B → v' ---- */
    uint8_t c2Local[F203_C2_BYTES];
    for (uint32_t b = 0; b < F203_C2_BYTES; ++b) {
        c2Local[b] = cIn[F203_C1_BYTES + b];
    }
    unpack_poly_v5(vLocal, c2Local);

    /* 写回 GM（本独立入口允许标量写；融合路径见 unpack_c_impl） */
    for (int32_t i = 0; i < kK * kN; ++i) {
        uOut[i] = uLocal[i];
    }
    for (int32_t i = 0; i < kN; ++i) {
        vOut[i] = vLocal[i];
    }
}

#ifndef __CCE_KT_TEST__
/** Host 侧 launch 包装。 */
void f203_decrypt_unpack_c_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *cGm, uint8_t *uGm,
                              uint8_t *vGm)
{
    f203_decrypt_unpack_c<<<blockDim, l2ctrl, stream>>>(cGm, uGm, vGm);
}
#endif
