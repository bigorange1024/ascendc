/**
 * @file f203_encrypt_g4_make_v_entry.cpp
 * @brief G4 子步：v = tr + e₂ + Decompress₁(μ) 嵌入（mod q）；tr 为 INTT 后首 poly[256]。
 */
#include "f203_encrypt_layout.h"
#include "kernel_operator.h"

using namespace AscendC;

namespace {

constexpr int32_t kN = F203_ENCRYPT_N;
constexpr int32_t kQ = F203_ENCRYPT_Q;
constexpr int32_t kHalfQ = (F203_ENCRYPT_Q + 1) / 2;

__aicore__ inline int32_t mod_q_add(int32_t a, int32_t b)
{
    int32_t x = a + b;
    x -= kQ * (x >= kQ);
    x += kQ * (x < 0);
    return x;
}

} // namespace

extern "C" __global__ __aicore__ void f203_encrypt_g4_make_v(GM_ADDR trGm, GM_ADDR e2Gm, GM_ADDR mGm, GM_ADDR vGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() != 0) {
        return;
    }
    const auto *tr = reinterpret_cast<const __gm__ int32_t *>(trGm);
    const auto *e2 = reinterpret_cast<const __gm__ int32_t *>(e2Gm);
    const auto *m = reinterpret_cast<const __gm__ uint8_t *>(mGm);
    auto *v = reinterpret_cast<__gm__ int32_t *>(vGm);
    for (int32_t c = 0; c < kN; ++c) {
        int32_t x = mod_q_add(tr[c], e2[c]);
        const int32_t i = c / 8;
        const int32_t j = c % 8;
        if (i < 32) {
            const int32_t bit = (static_cast<int32_t>(m[i]) >> j) & 1;
            x = mod_q_add(x, kHalfQ * bit);
        }
        v[c] = x;
    }
}

#ifndef __CCE_KT_TEST__
void f203_encrypt_g4_make_v_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *trGm, uint8_t *e2Gm,
                               uint8_t *mGm, uint8_t *vGm)
{
    f203_encrypt_g4_make_v<<<blockDim, l2ctrl, stream>>>(trGm, e2Gm, mGm, vGm);
}
#endif
