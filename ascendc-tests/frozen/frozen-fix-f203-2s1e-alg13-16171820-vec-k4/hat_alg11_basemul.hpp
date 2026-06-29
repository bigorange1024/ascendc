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
