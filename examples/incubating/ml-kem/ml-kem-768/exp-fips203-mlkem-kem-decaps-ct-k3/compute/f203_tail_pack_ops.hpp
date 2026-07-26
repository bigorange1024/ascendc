/**
 * @file f203_tail_pack_ops.hpp
 * @brief Alg.14 行 22–24 tail pack 可复用设备例程（ML-KEM-768，统一整数 Compress 向量 + ByteEncode 标量 pack）。
 * 供 f203_encrypt_alg14_pack（CPU 独立 launch）与 f203_encrypt_l18_l19（SIM 单 launch 内联 pack）共用。
 * 分片：subBlock0 → c₁[0..1]；subBlock1 → c₁[2]+c₂（与 INTT batch4 的 [u0,u1] / [u2,v] 对齐）。
 * Compress：统一整数 limb 宽乘（d=4/d=10 各 4×int32 scratch，无 float）。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-768（k=3）K-PKE.Encrypt。
 * 与 golden：最终对拍 output/c.bin。
 */
#pragma once

#include "f203_encrypt_tail_layout.h"
#include "f203_tail_compress_byteencode.hpp"
#include "kernel_operator.h"

namespace f203_tail {

constexpr uint32_t kPackN = F203_TAIL_N;
constexpr uint32_t kPackK = F203_TAIL_K;
constexpr uint32_t kCompressScratchElems = kPackN * 4U;

/** 将单 poly 系数 clamp 到 [0,q-1]（标量；INTT+噪声后偶发越界）。 */
__aicore__ inline void canonicalize_poly_q(AscendC::LocalTensor<int32_t> &poly)
{
    for (uint32_t i = 0; i < kPackN; ++i) {
        uint32_t u = static_cast<uint32_t>(poly.GetValue(static_cast<int32_t>(i)));
        if (u >= static_cast<uint32_t>(F203_TAIL_Q)) {
            u = static_cast<uint32_t>(F203_TAIL_Q) - 1U;
        }
        poly.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(u));
    }
}

/**
 * 单 poly u[p]：GM int32[256] → Compress₁₀ → ByteEncode₁₀ → c₁ 切片 320B。
 * @param gmU     u GM，行主序 k=3×256
 * @param gmC     密文 GM
 * @param polyIdx poly 下标 0..2
 */
__aicore__ inline void pack_one_u_poly_d10(AscendC::GlobalTensor<int32_t> &gmU, AscendC::GlobalTensor<uint8_t> &gmC,
                                           uint32_t polyIdx)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> quePoly;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queBytes;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufComp;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufScratch;

    pipe.InitBuffer(quePoly, 1, kPackN * sizeof(int32_t));
    pipe.InitBuffer(queBytes, 1, F203_TAIL_C1_POLY_BYTES);
    pipe.InitBuffer(bufComp, kPackN * sizeof(int32_t));
    pipe.InitBuffer(bufScratch, kCompressScratchElems * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> polyLocal = quePoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> compLocal = bufComp.Get<int32_t>();
    AscendC::LocalTensor<uint8_t> bytesLocal = queBytes.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> lo = bufScratch.GetWithOffset<int32_t>(kPackN, 0U);
    AscendC::LocalTensor<int32_t> hi = bufScratch.GetWithOffset<int32_t>(kPackN, kPackN * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> carry =
        bufScratch.GetWithOffset<int32_t>(kPackN, kPackN * 2U * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> scratch =
        bufScratch.GetWithOffset<int32_t>(kPackN, kPackN * 3U * sizeof(int32_t));

    AscendC::DataCopy(polyLocal, gmU[polyIdx * kPackN], kPackN);
    AscendC::PipeBarrier<PIPE_ALL>();
    canonicalize_poly_q(polyLocal);
    poly_compress_d10_vec(compLocal, polyLocal, lo, hi, carry, scratch);
    AscendC::PipeBarrier<PIPE_ALL>();
    poly_byte_encode_d10_local(bytesLocal, compLocal);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmC[polyIdx * F203_TAIL_C1_POLY_BYTES], bytesLocal, F203_TAIL_C1_POLY_BYTES);

    quePoly.FreeTensor(polyLocal);
    queBytes.FreeTensor(bytesLocal);
}

/** 单 poly v：GM int32[256] → Compress₄ → ByteEncode₄ → c₂ 128B（写 c 偏移 C1_BYTES）。 */
__aicore__ inline void pack_v_poly_d4(AscendC::GlobalTensor<int32_t> &gmV, AscendC::GlobalTensor<uint8_t> &gmC)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> quePoly;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queBytes;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufComp;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufScratch;

    pipe.InitBuffer(quePoly, 1, kPackN * sizeof(int32_t));
    pipe.InitBuffer(queBytes, 1, F203_TAIL_C2_BYTES);
    pipe.InitBuffer(bufComp, kPackN * sizeof(int32_t));
    pipe.InitBuffer(bufScratch, kCompressScratchElems * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> polyLocal = quePoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> compLocal = bufComp.Get<int32_t>();
    AscendC::LocalTensor<uint8_t> bytesLocal = queBytes.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> lo = bufScratch.GetWithOffset<int32_t>(kPackN, 0U);
    AscendC::LocalTensor<int32_t> hi = bufScratch.GetWithOffset<int32_t>(kPackN, kPackN * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> carry =
        bufScratch.GetWithOffset<int32_t>(kPackN, kPackN * 2U * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> scratch =
        bufScratch.GetWithOffset<int32_t>(kPackN, kPackN * 3U * sizeof(int32_t));

    AscendC::DataCopy(polyLocal, gmV, kPackN);
    AscendC::PipeBarrier<PIPE_ALL>();
    canonicalize_poly_q(polyLocal);
    poly_compress_d4_vec(compLocal, polyLocal, lo, hi, carry, scratch);
    AscendC::PipeBarrier<PIPE_ALL>();
    poly_byte_encode_d4_local(bytesLocal, compLocal);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmC[F203_TAIL_C1_BYTES], bytesLocal, F203_TAIL_C2_BYTES);

    quePoly.FreeTensor(polyLocal);
    queBytes.FreeTensor(bytesLocal);
}

/**
 * SIM 单 launch 内联 pack：按 subBlockID 分片写 c（u/v 已由本 AIV INTT+噪声写 GM）。
 * @param uGm       int32[3,256]
 * @param vGm       int32[256]（仅 subBlock1 读）
 * @param cGm       uint8[1088]；nullptr 则跳过
 * @param subBlockID 0 或 1
 */
__aicore__ inline void tail_pack_shard_gm(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cGm, int32_t subBlockID)
{
    if (cGm == nullptr) {
        return;
    }

    AscendC::GlobalTensor<int32_t> gmU;
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<uint8_t> gmC;
    gmU.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(uGm), kPackK * kPackN);
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(vGm), kPackN);
    gmC.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(cGm), F203_TAIL_C_BYTES);

    AscendC::PipeBarrier<PIPE_ALL>();

    if (subBlockID == 0) {
        pack_one_u_poly_d10(gmU, gmC, 0U);
        pack_one_u_poly_d10(gmU, gmC, 1U);
    } else if (subBlockID == 1) {
        pack_one_u_poly_d10(gmU, gmC, 2U);
        if (vGm != nullptr) {
            pack_v_poly_d4(gmV, gmC);
        }
    }
}

} // namespace f203_tail
