#ifndef F203_AIV_REORG_HPP
#define F203_AIV_REORG_HPP

#include "kernel_operator.h"

namespace f203_reorg {
constexpr int32_t kKPolys = 8;
constexpr int32_t kN = 256;

/** mat_a [HI(8);LO(8)] → interleaved [hi0,lo0,…,hi7,lo7] */
__aicore__ inline void InterleaveMatA(GM_ADDR matAGm, GM_ADDR interleavedGm, int32_t subBlockID)
{
    const int32_t polysPerSub = kKPolys / 2;
    const int32_t pStart = subBlockID * polysPerSub;

    AscendC::GlobalTensor<int8_t> srcHi;
    AscendC::GlobalTensor<int8_t> srcLo;
    AscendC::GlobalTensor<int8_t> dst;
    srcHi.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(matAGm), kKPolys * kN);
    srcLo.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(matAGm) + kKPolys * kN, kKPolys * kN);
    dst.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(interleavedGm), kKPolys * 2 * kN);

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf;
    pipe.InitBuffer(buf, static_cast<uint32_t>(kN * sizeof(int8_t)));
    AscendC::LocalTensor<int8_t> local = buf.Get<int8_t>(kN);

    for (int32_t p = pStart; p < pStart + polysPerSub; p++) {
        AscendC::DataCopy(local, srcHi[p * kN], kN);
        AscendC::DataCopy(dst[(2 * p) * kN], local, kN);
        AscendC::DataCopy(local, srcLo[p * kN], kN);
        AscendC::DataCopy(dst[(2 * p + 1) * kN], local, kN);
    }
}
} // namespace f203_reorg

#endif
