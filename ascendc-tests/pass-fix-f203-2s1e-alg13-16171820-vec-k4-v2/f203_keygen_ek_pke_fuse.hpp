/**
 * @file f203_keygen_ek_pke_fuse.hpp
 * @brief 行 21：在 mmad_custom 末尾将 ρ 拼至 ek_polyvec → ek_PKE（仅 F203_KEYGEN_EK_PKE=1）。
 *
 * 流水线位置：ByteEncode 写出 ek_out 之后、同次 Launch 内由 AIV0 调用。
 * 与 golden 关系：output/ek_pke.bin = ek_polyvec(1536) ‖ ρ(32)；verify 可选对拍。
 */
#pragma once

#include "f203_keygen_layout.h"
#include "kernel_operator.h"

namespace F203KeygenEkPke {

using namespace F203Keygen;

/**
 * ek_PKE = ByteEncode(t̂) ‖ ρ。
 * @param ek_polyvec_gm 已编码公钥 polyvec [1536] uint8
 * @param rho_gm        种子 ρ [32] uint8
 * @param ek_pke_gm     输出 [1568] uint8
 * 前置：仅 AIV subBlockID==0 且 runEncode；独立 TPipe，不破坏主 UB 融合管线。
 */
__aicore__ inline void FuseEkPke(GM_ADDR ek_polyvec_gm, GM_ADDR rho_gm, GM_ADDR ek_pke_gm)
{
    AscendC::GlobalTensor<uint8_t> ekSrc;
    ekSrc.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ek_polyvec_gm), kEkPolyvecBytes);
    AscendC::GlobalTensor<uint8_t> rhoSrc;
    rhoSrc.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(rho_gm), kRhoBytes);
    AscendC::GlobalTensor<uint8_t> ekDst;
    ekDst.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ek_pke_gm), kEkPkeBytes);

    /* 独立小 TPipe：避免与 Aiv2s1eUbPipeline 主 pipe 嵌套冲突 */
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufEk;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufRho;
    pipe.InitBuffer(bufEk, kEkPolyvecBytes);
    pipe.InitBuffer(bufRho, 32U);

    AscendC::LocalTensor<uint8_t> ekUb = bufEk.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> rhoUb = bufRho.Get<uint8_t>();

    /* GM→UB：ek 体 + ρ */
    AscendC::DataCopy(ekUb, ekSrc, kEkPolyvecBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(rhoUb, rhoSrc, kRhoBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    /* UB→GM：先写 1536B ek，再在偏移处追加 32B ρ */
    AscendC::DataCopy(ekDst, ekUb, kEkPolyvecBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(ekDst[kEkPolyvecBytes], rhoUb, kRhoBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
}

}  // namespace F203KeygenEkPke
