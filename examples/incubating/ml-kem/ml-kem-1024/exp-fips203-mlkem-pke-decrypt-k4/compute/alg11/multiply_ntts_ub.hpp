/**
 * @file multiply_ntts_ub.hpp
 * @brief Decrypt 流水线 Alg.11/12：在 UB 上计算 MultiplyNTTs(f,g)→h。
 *
 * 对齐 FIPS 203 Alg.11（MultiplyNTTs）/ Alg.12（BaseCaseMultiply）：
 *   对 i=0..127，用 γ_i 对 (f[2i],f[2i+1]) 与 (g[2i],g[2i+1]) 做 basemul。
 *
 * 本头被 su_dot（⟨ŝ,û⟩）调用；生产路径 ALG11_IMPL=1 走向量，CPU 调试走标量镜像。
 * golden I/O：无独立落盘；I/O 等价由 Decrypt 全链 m.bin 对拍覆盖。
 *
 * 开关见 multiply_ntts_config.hpp（ALG11_IMPL / VEC_VARIANT / MEM_OPS）。
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

/**
 * Barrett 约化到 [0,q=3329)：设备侧镜像参考 alg11_12_ref（须与 Host golden 同构）。
 * 两步 (×78>>18)、(×5039>>24) 后条件减 q。
 */
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

/**
 * Alg.12 BaseCaseMultiply：
 *   c0 = a0·b0 + a1·b1·γ；c1 = a0·b1 + a1·b0（均 mod q）。
 */
__aicore__ inline void base_case_multiply(int32_t *c0, int32_t *c1, int32_t a0, int32_t a1, int32_t b0, int32_t b1,
                                          int32_t gamma)
{
    int32_t a1b1 = barrett_red_coeff(a1 * b1);
    *c0 = barrett_red_coeff(a0 * b0 + a1b1 * gamma);
    *c1 = barrett_red_coeff(a0 * b1 + a1 * b0);
}

/**
 * 标量 Alg.11：对 N/2 对系数逐对 BaseCaseMultiply，γ 取自 kAlg11Gammas。
 * @param h 输出 [N]；@param f/@param g 输入 [N]
 */
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

/**
 * LocalTensor → 栈缓冲 → 标量 Alg.11 → 写回 LocalTensor。
 * 用于 ALG11_IMPL==0 或 ASCENDC_CPU_DEBUG（向量路径在 CPU 孪生上不可用时的回退）。
 */
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

/**
 * ALG11_IMPL=0：纯标量入口（无 ws / ROM 参数）。
 */
__aicore__ inline void compute_on_ub(AscendC::LocalTensor<int32_t> &hLocal,
                                     const AscendC::LocalTensor<int32_t> &fLocal,
                                     const AscendC::LocalTensor<int32_t> &gLocal)
{
    compute_on_ub_scalar(hLocal, fLocal, gLocal);
}

#elif ALG11_IMPL == 1

/**
 * 将 GM ROM（γ、Gather 偶/奇字节索引、interleave 重排）DataCopy 进已分配的 UB。
 * @param rom       调用方已 Alloc 的 LocalTensor 集合
 * @param pairCount 通常 128（N/2）
 */
__aicore__ inline void init_rom_luts_ub(alg11_vec::RomUbLuts &rom, int32_t pairCount)
{
    alg11_vec::init_rom_luts_ub(rom, pairCount);
}

/**
 * ALG11_IMPL=1：向量 MultiplyNTTs 入口。
 * @param hLocal 输出；@param fLocal/@param gLocal 输入 poly（已在 UB）
 * @param wsBase 向量工作区基址（8×pairCount int32）；@param rom 已 Init 的 ROM UB
 *
 * CPU 调试：忽略 ws/rom，回退 compute_on_ub_scalar。
 * 设备：bind_vec_ws → multiply_ntts_vec_dispatch（Gather deinterleave + 向量 Mul/Add）。
 */
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
