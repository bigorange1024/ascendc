/**
 * @file f203_decrypt_extract_m_entry.cpp
 * @brief 历史 G4 独立 kernel：v' − w_time → Compress₁ → m.bin（32B）。
 *
 * 流水线位置：旧多 launch 尾段；生产 fused 内联 tail_compress1_byteencode1。
 * Compress₁ 公式同 extract_impl（(Q+1)/2），非 Barrett 生产路径。
 * 与 golden：仅在旧 G4 对拍语境使用。
 */
#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

using namespace AscendC;

namespace {

constexpr int32_t kN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kQ = static_cast<int32_t>(F203_DECRYPT_Q);

/** (a−b) mod q。 */
__aicore__ inline int32_t mod_q_sub(int32_t a, int32_t b)
{
    int32_t x = a - b;
    x %= kQ;
    if (x < 0) {
        x += kQ;
    }
    return x;
}

/** 旧 FIPS 风格 Compress₁：输出 0/1（非 Barrett）。 */
__aicore__ inline uint32_t compress_1_u32(int32_t x)
{
    x = mod_q_sub(x, 0);
    const int32_t half = (kQ + 1) / 2;
    const int32_t t = (x << 1) + half;
    return static_cast<uint32_t>((t / kQ) & 1);
}

} // namespace

/**
 * 尾段 kernel 入口。
 * @param vGm / wTimeGm int32[256]；mGm uint8[32]
 * 前置：非 AIC；仅 block0。
 */
extern "C" __global__ __aicore__ void f203_decrypt_extract_m(GM_ADDR vGm, GM_ADDR wTimeGm, GM_ADDR mGm)
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

    const auto *vIn = reinterpret_cast<const __gm__ int32_t *>(vGm);
    const auto *wIn = reinterpret_cast<const __gm__ int32_t *>(wTimeGm);
    auto *mOut = reinterpret_cast<__gm__ uint8_t *>(mGm);

    int32_t vLocal[kN];
    int32_t wLocal[kN];
    for (int32_t i = 0; i < kN; ++i) {
        vLocal[i] = vIn[i];
        wLocal[i] = wIn[i];
    }

    uint8_t msg[F203_MSG_BYTES];
    for (uint32_t i = 0; i < F203_MSG_BYTES; ++i) {
        msg[i] = 0U;
    }
    for (int32_t i = 0; i < kN; ++i) {
        const int32_t w = mod_q_sub(vLocal[i], wLocal[i]);
        const uint32_t bit = compress_1_u32(w);
        const uint32_t byteIdx = static_cast<uint32_t>(i >> 3);
        const uint32_t bitIdx = static_cast<uint32_t>(i & 7);
        if (bit != 0U) {
            msg[byteIdx] |= static_cast<uint8_t>(1U << bitIdx);
        }
    }
    for (uint32_t i = 0; i < F203_MSG_BYTES; ++i) {
        mOut[i] = msg[i];
    }
}

#ifndef __CCE_KT_TEST__
/** Host 侧 launch 包装（非 KT 单测）。 */
void f203_decrypt_extract_m_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *vGm, uint8_t *wTimeGm,
                               uint8_t *mGm)
{
    f203_decrypt_extract_m<<<blockDim, l2ctrl, stream>>>(vGm, wTimeGm, mGm);
}
#endif
