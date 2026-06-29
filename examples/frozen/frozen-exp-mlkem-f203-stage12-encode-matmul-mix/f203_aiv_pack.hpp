#ifndef F203_AIV_PACK_HPP
#define F203_AIV_PACK_HPP

#include "kernel_operator.h"

namespace f203_pack {
constexpr int32_t kRows = 16;
constexpr int32_t kHalfCols = 256;
constexpr int32_t kOutCols = 512;
constexpr int32_t kRowsPerSub = kRows / 2;

/** A0||A1 [16,256] 各半 → mat_c [16,512] */
__aicore__ inline void PackMatCHalf(GM_ADDR matCGm, GM_ADDR a0Gm, GM_ADDR a1Gm, int32_t subBlockID)
{
    const int32_t rowStart = subBlockID * kRowsPerSub;
    AscendC::GlobalTensor<int32_t> cGm;
    AscendC::GlobalTensor<int32_t> hiGm;
    AscendC::GlobalTensor<int32_t> loGm;
    cGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(matCGm), kRows * kOutCols);
    hiGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(a0Gm), kRows * kHalfCols);
    loGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(a1Gm), kRows * kHalfCols);

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf;
    pipe.InitBuffer(buf, static_cast<uint32_t>(kHalfCols * sizeof(int32_t)));
    AscendC::LocalTensor<int32_t> local = buf.Get<int32_t>(kHalfCols);

    for (int32_t r = rowStart; r < rowStart + kRowsPerSub; r++) {
        AscendC::DataCopy(local, hiGm[r * kHalfCols], kHalfCols);
        AscendC::DataCopy(cGm[r * kOutCols], local, kHalfCols);
        AscendC::DataCopy(local, loGm[r * kHalfCols], kHalfCols);
        AscendC::DataCopy(cGm[r * kOutCols + kHalfCols], local, kHalfCols);
    }
}
} // namespace f203_pack

#endif
