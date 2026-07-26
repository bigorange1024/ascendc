/**
 * 【文件头】UB 上的 Alg.11/12 计算入口（标量镜像 + 向量分发）。
 *
 * 本文件在流水线中的位置：kernel Compute 调用的设备侧计算桥；
 *   ALG11_IMPL=0 走标量；=1 走 multiply_ntts_vec；CPU_DEBUG 强制标量对拍。
 * 作用：LocalTensor ↔ 栈缓冲的标量路径，以及向量路径的 init_rom / compute_on_ub。
 * 与 golden 关系：标量实现须与 alg11_12_ref.h 同步；向量路径 I/O 须与 golden_h.bin 一致。
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

/* 设备侧镜像 alg11_12_ref.h（须保持同步）。 */

/**
 * Barrett 模约化到 [0,q)（设备标量版）。
 * @param x  任意 int32 中间值（可负）
 * @return   x mod 3329，canonical
 */
__aicore__ inline int32_t barrett_red_coeff(int32_t x)
{
    const int32_t q = 3329;
    /* 负值抬升 */
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = (int32_t)(((int64_t)t * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = (int32_t)(((int64_t)x * 5039) >> 24);
    x = x - t2 * q;
    /* wrap_mod 末步 */
    x = x - (q & ~((x - q) >> 31));
    return x;
}

/**
 * Alg.12 BaseCaseMultiply（设备标量）。
 * @param c0,c1  输出偶/奇系数
 * @param a0,a1,b0,b1,gamma  输入对与 γ
 */
__aicore__ inline void base_case_multiply(int32_t *c0, int32_t *c1, int32_t a0, int32_t a1, int32_t b0, int32_t b1,
                                          int32_t gamma)
{
    int32_t a1b1 = barrett_red_coeff(a1 * b1);
    *c0 = barrett_red_coeff(a0 * b0 + a1b1 * gamma);
    *c1 = barrett_red_coeff(a0 * b1 + a1 * b0);
}

/**
 * Alg.11 MultiplyNTTs：AoS 多项式逐对标量乘。
 * @param h,f,g  各 [kN] int32，布局 2i/2i+1
 */
__aicore__ inline void multiply_ntts_scalar(int32_t *h, const int32_t *f, const int32_t *g)
{
    /* i：第 i 对；系数下标 2i / 2i+1 */
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
 * LocalTensor → 栈缓冲 → 标量 Alg.11/12 → 写回 LocalTensor。
 * @param hLocal  输出 UB [256] int32
 * @param fLocal  输入左 poly UB [256] int32
 * @param gLocal  输入右 poly UB [256] int32
 * 前置：三者已 Alloc；CPU_DEBUG 或 ALG11_IMPL=0 时使用。
 */
__aicore__ inline void compute_on_ub_scalar(AscendC::LocalTensor<int32_t> &hLocal,
                                            const AscendC::LocalTensor<int32_t> &fLocal,
                                            const AscendC::LocalTensor<int32_t> &gLocal)
{
    int32_t fBuf[alg11_tiling::kN];
    int32_t gBuf[alg11_tiling::kN];
    int32_t hBuf[alg11_tiling::kN];

    /* 从 UB 拉到栈，便于标量循环 */
    for (int32_t i = 0; i < alg11_tiling::kN; ++i) {
        fBuf[i] = fLocal.GetValue(i);
        gBuf[i] = gLocal.GetValue(i);
    }
    multiply_ntts_scalar(hBuf, fBuf, gBuf);
    /* 写回输出 UB */
    for (int32_t i = 0; i < alg11_tiling::kN; ++i) {
        hLocal.SetValue(i, hBuf[i]);
    }
}

#endif

#if ALG11_IMPL == 0

/**
 * ALG11_IMPL=0：UB 计算入口，直接标量路径。
 * @param hLocal,fLocal,gLocal  各 [256] int32 LocalTensor
 */
__aicore__ inline void compute_on_ub(AscendC::LocalTensor<int32_t> &hLocal,
                                     const AscendC::LocalTensor<int32_t> &fLocal,
                                     const AscendC::LocalTensor<int32_t> &gLocal)
{
    compute_on_ub_scalar(hLocal, fLocal, gLocal);
}

#elif ALG11_IMPL == 1

/**
 * Init：把 ROM LUT 物化到 UB（转发 alg11_vec）。
 * @param rom        RomUbLuts（γ / Gather / interleave 槽位）
 * @param pairCount  对数（128）
 */
__aicore__ inline void init_rom_luts_ub(alg11_vec::RomUbLuts &rom, int32_t pairCount)
{
    alg11_vec::init_rom_luts_ub(rom, pairCount);
}

/**
 * ALG11_IMPL=1：UB 计算入口。
 * @param hLocal  输出 [256] int32
 * @param fLocal,gLocal  输入 [256] int32
 * @param wsBase  向量工作区基址（a0..t2 等）
 * @param rom     已 Init 的 ROM LUT
 * 分支：CPU_DEBUG 走标量；否则 bind_vec_ws + multiply_ntts_vec_dispatch。
 */
__aicore__ inline void compute_on_ub(AscendC::LocalTensor<int32_t> &hLocal,
                                     const AscendC::LocalTensor<int32_t> &fLocal,
                                     const AscendC::LocalTensor<int32_t> &gLocal,
                                     AscendC::LocalTensor<int32_t> &wsBase, const alg11_vec::RomUbLuts &rom)
{
#if defined(ASCENDC_CPU_DEBUG)
    /* tikicpu：向量 Gather/DataCopy 事件易残留，用标量保证对拍 */
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
