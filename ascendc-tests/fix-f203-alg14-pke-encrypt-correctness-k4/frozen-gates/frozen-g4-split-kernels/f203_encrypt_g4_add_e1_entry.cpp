/**
 * @file f203_encrypt_g4_add_e1_entry.cpp
 * @brief G4 子步：时域 u[4,256] += e₁（mod q，in-place）。SIM 规避六参 g4_noise launch 507000。
 */
#include "f203_encrypt_layout.h"
#include "kernel_operator.h"

using namespace AscendC;

namespace {

constexpr int32_t kN = F203_ENCRYPT_N;
constexpr int32_t kK = F203_ENCRYPT_K;
constexpr int32_t kQ = F203_ENCRYPT_Q;

} // namespace

extern "C" __global__ __aicore__ void f203_encrypt_g4_add_e1(GM_ADDR uGm, GM_ADDR e1Gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() != 0) {
        return;
    }
    auto *u = reinterpret_cast<__gm__ int32_t *>(uGm);
    const auto *e1 = reinterpret_cast<const __gm__ int32_t *>(e1Gm);
    for (int32_t p = 0; p < kK; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            const int32_t idx = p * kN + c;
            int32_t x = u[idx] + e1[idx];
            x -= kQ * (x >= kQ);
            x += kQ * (x < 0);
            u[idx] = x;
        }
    }
}

#ifndef __CCE_KT_TEST__
void f203_encrypt_g4_add_e1_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uGm, uint8_t *e1Gm)
{
    f203_encrypt_g4_add_e1<<<blockDim, l2ctrl, stream>>>(uGm, e1Gm);
}
#endif
