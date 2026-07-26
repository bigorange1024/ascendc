// @probe pass-fix-f203-alg19-kem-keygen-device-k3
// @file compute/multiply_ntts_ub.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `multiply_ntts_ub.hpp` 为该子模块组件。 / Component: multiply_ntts_ub.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_kem.bin (1184B) + dk_kem.bin (2400B)；D13 PKE 中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_kem+dk_kem out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: kernel_operator.h, multiply_ntts_config.hpp, alg11_tiling.h, multiply_ntts_vec.hpp, alg11_gammas.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 NTT 后乘积 / 配置辅助。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/multiply_ntts_ub.hpp
 */
/**
 * @file multiply_ntts_ub.hpp
 * @brief Alg.11 MultiplyNTTs 的 UB 门面：标量回退 + 向量分发（compute_on_ub / init_rom_luts_ub）。
 *
 * 用途：在 LocalTensor 上完成 f∘g→h（256 系数交错布局）；CPU 孪生强制标量路径保证与 C ref 一致。
 *
 * 调用方：`2s1e_post_ntt_ub.hpp`（j→p 全 poly）、`hat_dot_halfrows_ub.hpp`；向量细节在 multiply_ntts_vec.hpp。
 *
 * 不变量：
 *   - ALG11_IMPL=0 仅标量；ALG11_IMPL=1 时设备走 alg11_vec::multiply_ntts_vec_dispatch；
 *   - ASCENDC_CPU_DEBUG 下即使 ALG11_IMPL=1 也走 compute_on_ub_scalar；
 *   - 热路径禁止在 Compute 阶段 SetValue 填 γ/索引（ALG11_MEM_OPS=1 用 ROM Init）。
 *
 * Golden：逐系数由 hat_multiply_ntts 隐式覆盖；整行 18 对拍 golden_t_hat_*.bin。
 *
 * CMake：ALG11_IMPL、ALG11_MEM_OPS 等见 multiply_ntts_config.hpp。
 */
#pragma once

#include "kernel_operator.h"
#include "multiply_ntts_config.hpp"
#include "alg11_tiling.h"

#if ALG11_IMPL == 1
#include "multiply_ntts_vec.hpp"
#endif

namespace alg11_ub {

#if ALG11_IMPL == 0 || defined(ASCENDC_CPU_DEBUG)

#include "alg11_gammas.h"

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
 * 本函数为 KeyGen 流水线组件 `base_case_multiply`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-768（k=3）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void base_case_multiply(int32_t *c0, int32_t *c1, int32_t a0, int32_t a1, int32_t b0, int32_t b1,
                                          int32_t gamma)
{
    int32_t a1b1 = barrett_red_coeff(a1 * b1);
    *c0 = barrett_red_coeff(a0 * b0 + a1b1 * gamma);
    *c1 = barrett_red_coeff(a0 * b1 + a1 * b0);
}

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
 * 本函数为 KeyGen 流水线组件 `compute_on_ub_scalar`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-768（k=3）；与 golden 仅 I/O 等价。
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

/** ALG11_IMPL=0：标量 multiply_ntts_scalar 门面 */
__aicore__ inline void compute_on_ub(AscendC::LocalTensor<int32_t> &hLocal,
                                     const AscendC::LocalTensor<int32_t> &fLocal,
                                     const AscendC::LocalTensor<int32_t> &gLocal)
{
    compute_on_ub_scalar(hLocal, fLocal, gLocal);
}

#elif ALG11_IMPL == 1

/** GM ROM → UB（Init 一次）；设备热路径入口 */
__aicore__ inline void init_rom_luts_ub(alg11_vec::RomUbLuts &rom, int32_t pairCount)
{
    alg11_vec::init_rom_luts_ub(rom, pairCount);
}

/** f∘g→h 单步 NTT-Mul；CPU_DEBUG 强制标量，设备走 multiply_ntts_vec_dispatch */
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

} // namespace alg11_ub
