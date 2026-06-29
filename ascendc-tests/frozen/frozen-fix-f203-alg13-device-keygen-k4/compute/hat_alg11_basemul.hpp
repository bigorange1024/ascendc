// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/hat_alg11_basemul.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `hat_alg11_basemul.hpp` 为该子模块组件。 / Component: hat_alg11_basemul.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: alg11_rom_tables.h, alg11_ub_load.hpp, integration_config.hpp, kernel_operator.h, multiply_ntts_vec.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

/**
 * @file hat_alg11_basemul.hpp
 * @brief 行 18 专用：半多项式（128 对）MultiplyNTTs 向量封装与 ROM/ws 绑定。
 *
 * 用途：在已有 RomUbLuts + VecWs 上执行 multiply_ntts_half_vec；支持 gammaOff 切片（lo/hi 半核 γ 偏移 0/64）。
 *
 * 调用方：`2s1e_post_ntt_ub.hpp`（HAT_ALG11_VEC=1 && HAT_LINE18_FULLPOLY=1 的 j→p 内积环）。
 *
 * 不变量：
 *   - halfLen=128 交错系数；γ 来自 gAlg11GammasGm 或 UB ROM；
 *   - gammaOff≠0 时 copy_rom_int32_ub 后 ALG11_PIPE_MTE2；
 *   - 与 alg11_vec::multiply_ntts_vec_dispatch 语义一致。
 *
 * Golden：hat_inner_product_ref.c 全长 256 basemul；行 18 对拍 t_hat。
 *
 * CMake：HAT_ALG11_VEC、ALG11_*（integration_config.hpp / CMakeLists.txt）。
 */
#ifndef HAT_ALG11_BASEMUL_HPP
#define HAT_ALG11_BASEMUL_HPP

#include "alg11_rom_tables.h"
#include "alg11_ub_load.hpp"
#include "integration_config.hpp"
#include "kernel_operator.h"
#include "multiply_ntts_vec.hpp"

namespace hat_alg11 {

__aicore__ inline void bind_rom_ub(AscendC::LocalTensor<int32_t> &base, alg11_vec::RomUbLuts &rom, int32_t pairCount)
{
    rom.gammaV = base[0];
    rom.gatherEvenByte = base[pairCount];
    rom.gatherOddByte = base[2 * pairCount];
    rom.interleaveReorderByte = base[3 * pairCount];
    (void)pairCount;
}

__aicore__ inline void init_rom_luts(AscendC::LocalTensor<int32_t> &romBase, alg11_vec::RomUbLuts &rom,
                                     int32_t pairCount)
{
    bind_rom_ub(romBase, rom, pairCount);
    alg11_vec::init_rom_luts_ub(rom, pairCount);
}

__aicore__ inline void bind_basemul_ws(AscendC::LocalTensor<int32_t> &base, alg11_vec::VecWs &w,
                                       alg11_vec::RomUbLuts &rom, int32_t pairCount)
{
    alg11_vec::bind_vec_ws(base, w, pairCount, rom);
}

/**
 * 半多项式 MultiplyNTTs（128 对），γ 从 gammaOff 起（0 或 64）。
 * f/g/h 长度 halfLen=128（交错系数）。
 */
__aicore__ inline void multiply_ntts_half_vec(AscendC::LocalTensor<int32_t> &h,
                                              const AscendC::LocalTensor<int32_t> &f,
                                              const AscendC::LocalTensor<int32_t> &g, alg11_vec::VecWs &w,
                                              alg11_vec::RomUbLuts &rom, AscendC::LocalTensor<int32_t> &gammaSlice,
                                              int32_t pairCount, int32_t gammaOff)
{
    if (gammaOff != 0) {
        alg11_ub_load::copy_rom_int32_ub(gammaSlice, gAlg11GammasGm + gammaOff, pairCount);
        ALG11_PIPE_MTE2();
        w.gammaV = gammaSlice;
    } else {
        w.gammaV = rom.gammaV;
    }
    alg11_vec::multiply_ntts_vec_dispatch(h, f, g, w, rom, pairCount);
    w.gammaV = rom.gammaV;
}

} // namespace hat_alg11

#endif
