#pragma once

/**
 * @file mod_q_add.hpp
 * @brief Z_q 向量模加：先 Add 再 Barrett mod q（UB / GM 两种落点）。
 *
 * 用途：行 19 u←INTT(û)+e₁、行 18 +ê 等；依赖 mod_q_vec.hpp。
 * 调用方：encrypt-compute 探针 INTT 后加噪；其它用例 include 后直接调用。
 *
 * 不变量：行优先 [poly, coeff]；halfrows 每 AIV 处理连续 poly 行 [pBegin,pEnd)。
 */
#include "f203_mod_q/mod_q_vec.hpp"
#include "kernel_operator.h"

namespace f203_mod_q {

/** UB 原地：dst ← (dst + src) mod q。mod 临时区 t1/t2 各 ≥ count。 */
__aicore__ inline void mod_q_add_ub_inplace(AscendC::LocalTensor<int32_t> &dst, AscendC::LocalTensor<int32_t> &src,
                                            int32_t q, AscendC::LocalTensor<int32_t> &t1,
                                            AscendC::LocalTensor<int32_t> &t2, int32_t count)
{
    AscendC::Add(dst, dst, src, count);
    AscendC::PipeBarrier<PIPE_ALL>();
#if defined(ASCENDC_CPU_DEBUG)
    mod_q_final_vec(dst, q, count);
#else
    mod_q_barrett_vec(dst, q, t1, t2, count);
    AscendC::PipeBarrier<PIPE_ALL>();
#endif
}

/**
 * GM 分片模加：dst[p,c] ← (lhs[p,c] + rhs[p,c]) mod q，p∈[pBegin,pEnd)。
 * SIM：按 poly 行 DataCopy→向量 Add→Barrett mod→写回；禁止标量写 GM。
 */
__aicore__ inline void mod_q_add_gm_polyrows(GM_ADDR dstGm, GM_ADDR lhsGm, GM_ADDR rhsGm, int32_t q, int32_t pBegin,
                                             int32_t pEnd, int32_t coeffN)
{
#if defined(ASCENDC_CPU_DEBUG)
    auto *dGm = reinterpret_cast<__gm__ int32_t *>(dstGm);
    const auto *lGm = reinterpret_cast<const __gm__ int32_t *>(lhsGm);
    const auto *rGm = reinterpret_cast<const __gm__ int32_t *>(rhsGm);
    for (int32_t p = pBegin; p < pEnd; ++p) {
        for (int32_t c = 0; c < coeffN; ++c) {
            const uint32_t off = static_cast<uint32_t>(p) * static_cast<uint32_t>(coeffN) + static_cast<uint32_t>(c);
            int32_t v = lGm[off] + rGm[off];
            v %= q;
            if (v < 0) {
                v += q;
            }
            dGm[off] = v;
        }
    }
#else
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufLine;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufRhs;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufT1;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufT2;
    const uint32_t lineBytes = static_cast<uint32_t>(coeffN) * sizeof(int32_t);
    pipe.InitBuffer(bufLine, lineBytes);
    pipe.InitBuffer(bufRhs, lineBytes);
    pipe.InitBuffer(bufT1, lineBytes);
    pipe.InitBuffer(bufT2, lineBytes);

    AscendC::LocalTensor<int32_t> lineUb = bufLine.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> rhsUb = bufRhs.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> t1 = bufT1.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> t2 = bufT2.AllocTensor<int32_t>();

    AscendC::GlobalTensor<int32_t> dstG;
    AscendC::GlobalTensor<int32_t> lhsG;
    AscendC::GlobalTensor<int32_t> rhsG;
    dstG.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dstGm));
    lhsG.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(lhsGm));
    rhsG.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(rhsGm));

    const uint32_t lineLen = static_cast<uint32_t>(coeffN);
    for (int32_t p = pBegin; p < pEnd; ++p) {
        const uint32_t off = static_cast<uint32_t>(p) * lineLen;
        AscendC::DataCopy(lineUb, lhsG[off], lineLen);
        AscendC::DataCopy(rhsUb, rhsG[off], lineLen);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::Add(lineUb, lineUb, rhsUb, static_cast<int32_t>(lineLen));
        AscendC::PipeBarrier<PIPE_ALL>();
        mod_q_barrett_vec(lineUb, q, t1, t2, static_cast<int32_t>(lineLen));
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(dstG[off], lineUb, lineLen);
        AscendC::PipeBarrier<PIPE_ALL>();
    }
#endif
}

/** halfrows 便捷：subBlockID×2 起连续 2 个 poly 行。 */
__aicore__ inline void mod_q_add_gm_halfrows(GM_ADDR dstGm, GM_ADDR lhsGm, GM_ADDR rhsGm, int32_t subBlockID,
                                             int32_t coeffN, int32_t q)
{
    const int32_t pBegin = subBlockID * 2;
    const int32_t pEnd = pBegin + 2;
    mod_q_add_gm_polyrows(dstGm, lhsGm, rhsGm, q, pBegin, pEnd, coeffN);
}

/** GM 单行模加：dst ← (dst + rhs) mod q（用于 v ← INTT(tr̂)+e₂）。 */
__aicore__ inline void mod_q_add_gm_single_row(GM_ADDR dstGm, GM_ADDR lhsGm, GM_ADDR rhsGm, int32_t q, int32_t coeffN)
{
    mod_q_add_gm_polyrows(dstGm, lhsGm, rhsGm, q, 0, 1, coeffN);
}

} // namespace f203_mod_q
