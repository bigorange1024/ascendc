/**
 * @file f203_kem_dec_pack_entry.cpp
 * @brief Alg.21 Decaps Phase-E 尾：Compress/ByteEncode 写 c'，同核嵌入设备 FO。
 *
 * 相对 vendor Encrypt pack：多传入 c_in / z / K' / Kout，在 pack 完成后调用 KemDecFo。
 * 保证 CPU 与 SIM 生产路径 FO 均在设备完成（无 host memcmp）。
 * Compress d=11(u) / d=5(v) 标量路径与 Encrypt pack 语义对齐。
 */
#include "f203_encrypt_layout.h"
#include "f203_encrypt_pack_config.hpp"
#include "f203_kem_dec_fo.hpp"
#include "kernel_operator.h"

using namespace AscendC;

namespace {

constexpr int32_t kN = F203_ENCRYPT_N;
constexpr int32_t kK = F203_ENCRYPT_K;

/** Compress₅：系数 → 5 bit（v 多项式）。 */
__aicore__ inline uint32_t compress_d5_u32(uint32_t u)
{
    uint32_t d0 = u * 1290176u;
    return ((d0 + (1u << 26)) >> 27) & 0x1fu;
}

/** Compress₁₁：系数 → 11 bit（u 多项式）。 */
__aicore__ inline uint32_t compress_d11_u32(uint32_t u)
{
    uint64_t d0 = static_cast<uint64_t>(u) * 5284526080ull;
    d0 = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<uint32_t>(d0 & 0x7ffu);
}

/** 将 N 个 dBits 宽系数按小端比特序打包到 GM。 */
__aicore__ inline void byte_encode_bits_scalar(__gm__ uint8_t *out, const int32_t *comp, uint32_t dBits)
{
    uint32_t bitPos = 0U;
    uint8_t cur = 0U;
    uint32_t outIdx = 0U;
    const uint32_t mask = (dBits >= 32U) ? 0xFFFFFFFFu : ((1U << dBits) - 1U);
    for (uint32_t i = 0; i < static_cast<uint32_t>(kN); ++i) {
        uint32_t a = static_cast<uint32_t>(comp[i]) & mask;
        for (uint32_t j = 0; j < dBits; ++j) {
            if ((a >> j) & 1U) {
                cur |= static_cast<uint8_t>(1U << (bitPos & 7U));
            }
            ++bitPos;
            if ((bitPos & 7U) == 0U) {
                out[outIdx++] = cur;
                cur = 0U;
            }
        }
    }
    if ((bitPos & 7U) != 0U) {
        out[outIdx] = cur;
    }
}

/** 单 poly：饱和到 q 后 Compress₁₁ + ByteEncode → c1 片段。 */
__aicore__ inline void pack_one_poly_u11(__gm__ uint8_t *out, const __gm__ int32_t *polyIn)
{
    int32_t comp[kN];
    for (int32_t i = 0; i < kN; ++i) {
        uint32_t u = static_cast<uint32_t>(polyIn[i]);
        if (u >= static_cast<uint32_t>(F203_ENCRYPT_Q)) {
            u = static_cast<uint32_t>(F203_ENCRYPT_Q) - 1U;
        }
        comp[i] = static_cast<int32_t>(compress_d11_u32(u));
    }
    byte_encode_bits_scalar(out, comp, 11U);
}

/** 单 poly：饱和到 q 后 Compress₅ + ByteEncode → c2 片段。 */
__aicore__ inline void pack_one_poly_v5(__gm__ uint8_t *out, const __gm__ int32_t *polyIn)
{
    int32_t comp[kN];
    for (int32_t i = 0; i < kN; ++i) {
        uint32_t u = static_cast<uint32_t>(polyIn[i]);
        if (u >= static_cast<uint32_t>(F203_ENCRYPT_Q)) {
            u = static_cast<uint32_t>(F203_ENCRYPT_Q) - 1U;
        }
        comp[i] = static_cast<int32_t>(compress_d5_u32(u));
    }
    byte_encode_bits_scalar(out, comp, 5U);
}

}  // namespace

/**
 * 设备核：pack u/v → c'，再 KemDecFo(c,c',z,K')→Kout。
 * CPU：AIV_ONLY；SIM：MIX 占位，仅 AIV block0 执行。
 */
extern "C" __global__ __aicore__ void f203_kem_dec_pack(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cPrimeGm, GM_ADDR cInGm,
                                                        GM_ADDR zGm, GM_ADDR KprimeGm, GM_ADDR KoutGm)
{
#if defined(ASCENDC_CPU_DEBUG)
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() != 0) {
        return;
    }
#else
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    if (AscendC::GetSubBlockNum() == 1) {
        return;
    }
    if (GetBlockIdx() != 0) {
        return;
    }
#endif
    // ① Compress/ByteEncode：c' = ByteEncode₁₁(u)‖ByteEncode₅(v)
    const auto *uIn = reinterpret_cast<const __gm__ int32_t *>(uGm);
    const auto *vIn = reinterpret_cast<const __gm__ int32_t *>(vGm);
    auto *cOut = reinterpret_cast<__gm__ uint8_t *>(cPrimeGm);

    for (int32_t p = 0; p < kK; ++p) {
        pack_one_poly_u11(cOut + static_cast<uint32_t>(p) * F203_C1_POLY_BYTES, uIn + p * kN);
    }
    pack_one_poly_v5(cOut + F203_C1_BYTES, vIn);

    // ② 设备 FO：c vs c' → K 或 J(z‖c)
    F203KemDec::KemDecFo(reinterpret_cast<__gm__ uint8_t *>(cInGm), cOut, reinterpret_cast<__gm__ uint8_t *>(zGm),
                         reinterpret_cast<__gm__ uint8_t *>(KprimeGm), reinterpret_cast<__gm__ uint8_t *>(KoutGm));
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_kem_dec_pack_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uGm, uint8_t *vGm,
                                     uint8_t *cPrimeGm, uint8_t *cInGm, uint8_t *zGm, uint8_t *KprimeGm,
                                     uint8_t *KoutGm)
{
    f203_kem_dec_pack<<<blockDim, l2ctrl, stream>>>(uGm, vGm, cPrimeGm, cInGm, zGm, KprimeGm, KoutGm);
}
#endif
