/**
 * @file f203_keygen_ek_pke_fuse.hpp
 * @brief 行 21：在 Launch 2（mmad）末尾将 ρ 拼至 ek_polyvec → ek_PKE（仅 F203_KEYGEN_EK_PKE=1）。
 */
#pragma once

#include "f203_keygen_layout.h"
#include "kernel_operator.h"

namespace F203KeygenEkPke {

using namespace F203Keygen;

__aicore__ inline void FuseEkPke(GM_ADDR ek_polyvec_gm, GM_ADDR rho_gm, GM_ADDR ek_pke_gm)
{
    AscendC::GlobalTensor<uint8_t> ekSrc;
    ekSrc.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ek_polyvec_gm), kEkPolyvecBytes);
    AscendC::GlobalTensor<uint8_t> rhoSrc;
    rhoSrc.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(rho_gm), kRhoBytes);
    AscendC::GlobalTensor<uint8_t> ekDst;
    ekDst.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ek_pke_gm), kEkPkeBytes);

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufEk;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufRho;
    pipe.InitBuffer(bufEk, kEkPolyvecBytes);
    pipe.InitBuffer(bufRho, 32U);

    AscendC::LocalTensor<uint8_t> ekUb = bufEk.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> rhoUb = bufRho.Get<uint8_t>();

    AscendC::DataCopy(ekUb, ekSrc, kEkPolyvecBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(rhoUb, rhoSrc, kRhoBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(ekDst, ekUb, kEkPolyvecBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(ekDst[kEkPolyvecBytes], rhoUb, kRhoBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
}

}  // namespace F203KeygenEkPke
