/**
 * @file multiply_ntts_ub.hpp
 * @brief Alg.11 UB 入口：按 ALG11_IMPL 分派标量或向量 MultiplyNTTs。
 *
 * 流水线位置：su_dot_impl / su_dot_kernel。
 * CPU debug 强制标量路径；设备生产走 multiply_ntts_vec。
 * 与 golden：同 multiply_ntts 数学契约。
 */
#pragma once

#include "kernel_operator.h"
#include "multiply_ntts_config.hpp"
#include "tiling.h"

#if ALG11_IMPL == 1
#include "multiply_ntts_vec.hpp"
#endif

namespace alg11_ub {

#if ALG11_IMPL == 0 || defined(ASCENDC_CPU_DEBUG)

#include "alg11_gammas.h"

/** 设备侧 Barrett（与 Host barrett_red / alg11_12_ref 同步）。 */
__aicore__ inline int32_t barrett_red_coeff(int32_t x)
{
    const int32_t q = 3329;
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = (int32_t)(((int64_t)t * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = (int32_t)(((int64_t)x * 5039) >> 24);
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

/** Alg.12 基域一对： (a0+a1X)(b0+b1X) 在 γ 下。 */
__aicore__ inline void base_case_multiply(int32_t *c0, int32_t *c1, int32_t a0, int32_t a1, int32_t b0, int32_t b1,
                                          int32_t gamma)
{
    int32_t a1b1 = barrett_red_coeff(a1 * b1);
    *c0 = barrett_red_coeff(a0 * b0 + a1b1 * gamma);
    *c1 = barrett_red_coeff(a0 * b1 + a1 * b0);
}

/** 标量 MultiplyNTTs：128 对基域乘。 */
__aicore__ inline void multiply_ntts_scalar(int32_t *h, const int32_t *f, const int32_t *g)
{
    for (int i = 0; i < alg11_tiling::kN / 2; ++i) {
        int32_t a0 = f[i * 2];
        int32_t a1 = f[i * 2 + 1];
        int32_t b0 = g[i * 2];
        int32_t b1 = g[i * 2 + 1];
        int32_t c0 = 0;
        int32_t c1 = 0;
        base_case_multiply(&c0, &c1, a0, a1, b0, b1, kAlg11Gammas[i]);
        h[i * 2] = c0;
        h[i * 2 + 1] = c1;
    }
}

/** LocalTensor → stack → scalar Alg.11/12 → LocalTensor */
__aicore__ inline void compute_on_ub_scalar(AscendC::LocalTensor<int32_t> &hLocal,
                                            const AscendC::LocalTensor<int32_t> &fLocal,
                                            const AscendC::LocalTensor<int32_t> &gLocal)
{
    int32_t fBuf[alg11_tiling::kN];
    int32_t gBuf[alg11_tiling::kN];
    int32_t hBuf[alg11_tiling::kN];

    for (int32_t i = 0; i < alg11_tiling::kN; ++i) {
        fBuf[i] = fLocal.GetValue(i);
        gBuf[i] = gLocal.GetValue(i);
    }
    multiply_ntts_scalar(hBuf, fBuf, gBuf);
    for (int32_t i = 0; i < alg11_tiling::kN; ++i) {
        hLocal.SetValue(i, hBuf[i]);
    }
}

#endif

#if ALG11_IMPL == 0

__aicore__ inline void compute_on_ub(AscendC::LocalTensor<int32_t> &hLocal,
                                     const AscendC::LocalTensor<int32_t> &fLocal,
                                     const AscendC::LocalTensor<int32_t> &gLocal)
{
    compute_on_ub_scalar(hLocal, fLocal, gLocal);
}

#elif ALG11_IMPL == 1

__aicore__ inline void init_rom_luts_ub(alg11_vec::RomUbLuts &rom, int32_t pairCount)
{
    alg11_vec::init_rom_luts_ub(rom, pairCount);
}

__aicore__ inline void compute_on_ub(AscendC::LocalTensor<int32_t> &hLocal,
                                     const AscendC::LocalTensor<int32_t> &fLocal,
                                     const AscendC::LocalTensor<int32_t> &gLocal,
                                     AscendC::LocalTensor<int32_t> &wsBase, const alg11_vec::RomUbLuts &rom)
{
#if defined(ASCENDC_CPU_DEBUG)
    (void)wsBase;
    (void)rom;
    compute_on_ub_scalar(hLocal, fLocal, gLocal);
#else
    alg11_vec::VecWs w;
    alg11_vec::bind_vec_ws(wsBase, w, alg11_vec::kPairCount, rom);
    alg11_vec::multiply_ntts_vec_dispatch(hLocal, fLocal, gLocal, w, rom, alg11_vec::kPairCount);
#endif
}

#else
#error "unsupported ALG11_IMPL"
#endif

}  // namespace alg11_ub
