/**
 * @file f203_encrypt_re_vector.hpp
 * @brief Alg.14 G1 Launch-2 编排：coins → PRF batch9 → CBD → r/e1/e2 GM。
 */
#pragma once

#include "f203_encrypt_re_cbd.hpp"
#include "f203_encrypt_re_prf.hpp"

namespace F203EncryptRe {

__aicore__ inline void BuildReFromCoinsGm(const __gm__ uint8_t *coins_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                           GM_ADDR tiling_gm)
{
    BuildPrfOutFromCoinsGm(coins_gm, prf_out_gm, tiling_gm);
    AscendC::PipeBarrier<PIPE_ALL>();
    SampleCbdBatch9FromPrfGm(reinterpret_cast<const __gm__ uint8_t *>(prf_out_gm),
                             reinterpret_cast<__gm__ int32_t *>(re_gm));
}

}  // namespace F203EncryptRe
