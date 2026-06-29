#ifndef __AIV_FUNC_HPP__
#define __AIV_FUNC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "ntt_vec.hpp"
#include "tiling.h"

using AscendC::DataCopy;

namespace planar {

__aicore__ inline uint32_t s0_hi_row(uint32_t sub, uint32_t lp)
{
    return sub * static_cast<uint32_t>(tiling::s0RowsPerAiv) + lp;
}

__aicore__ inline uint32_t s0_lo_row(uint32_t sub, uint32_t lp)
{
    return sub * static_cast<uint32_t>(tiling::s0RowsPerAiv) + static_cast<uint32_t>(tiling::kPolysPerAiv) + lp;
}

/** 平面 mat_c 行号：half 0=C_lo，1=C_hi；limb 0..3 = hh,lh,hl,ll。 */
__aicore__ inline uint32_t mat_row(uint32_t polyIdx, uint32_t limb, uint32_t half)
{
    return half * static_cast<uint32_t>(tiling::kPolys) * tiling::kLimbsPerPoly + polyIdx * tiling::kLimbsPerPoly +
           limb;
}

} // namespace planar

class AivSplitPolyBatch {
public:
    __aicore__ inline AivSplitPolyBatch(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx(subCoreIdx), coeffN(coeffN),
          kPolysLocal(static_cast<uint16_t>(tiling::kPolysPerAiv)),
          polyStart(static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kPolysPerAiv)),
          tileLength(static_cast<uint32_t>(tiling::kPolysPerAiv) * coeffN)
    {
    }

    __aicore__ inline void Init(GM_ADDR wsS0, GM_ADDR src)
    {
        gm_src.SetGlobalBuffer((__gm__ int32_t *)src);
        gm_s0.SetGlobalBuffer((__gm__ int8_t *)wsS0);
        pipe.InitBuffer(in_src, 1, tileLength * sizeof(int32_t));
        pipe.InitBuffer(out_dst0, 1, tileLength * sizeof(int8_t));
        pipe.InitBuffer(out_dst1, 1, tileLength * sizeof(int8_t));
    }

    __aicore__ inline void CopyIn()
    {
        LocalTensor<int32_t> local_src = in_src.AllocTensor<int32_t>();
        const uint32_t srcBase = static_cast<uint32_t>(polyStart) * coeffN;
        DataCopy(local_src, gm_src[srcBase], tileLength);
        KYBER_PIPE_ALL();
        in_src.EnQue(local_src);
    }

    __aicore__ inline void Compute()
    {
        LocalTensor<int32_t> local_src = in_src.DeQue<int32_t>();
        LocalTensor<int8_t> local_dst0 = out_dst0.AllocTensor<int8_t>();
        LocalTensor<int8_t> local_dst1 = out_dst1.AllocTensor<int8_t>();
        Tensor_int8x4 res{local_dst0, local_dst1};
        split_vec(res, local_src, static_cast<int32_t>(tileLength));
        out_dst0.EnQue<int8_t>(local_dst0);
        out_dst1.EnQue<int8_t>(local_dst1);
        in_src.FreeTensor(local_src);
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void CopyOut()
    {
        LocalTensor<int8_t> local_dst0 = out_dst0.DeQue<int8_t>();
        LocalTensor<int8_t> local_dst1 = out_dst1.DeQue<int8_t>();
        const uint32_t rowBase = static_cast<uint32_t>(subCoreIdx) * static_cast<uint32_t>(tiling::s0RowsPerAiv);
        const uint32_t hiBase = rowBase * coeffN;
        const uint32_t loBase = (rowBase + static_cast<uint32_t>(tiling::kPolysPerAiv)) * coeffN;
        DataCopy(gm_s0[hiBase], local_dst1, tileLength);
        DataCopy(gm_s0[loBase], local_dst0, tileLength);
        KYBER_PIPE_ALL();
        out_dst0.FreeTensor(local_dst0);
        out_dst1.FreeTensor(local_dst1);
    }

    __aicore__ inline void Process()
    {
        CopyIn();
        Compute();
        CopyOut();
    }

private:
    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint16_t kPolysLocal;
    const uint16_t polyStart;
    const uint32_t tileLength;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> in_src;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst0, out_dst1;
    AscendC::GlobalTensor<int32_t> gm_src;
    AscendC::GlobalTensor<int8_t> gm_s0;
};

/**
 * Stage2 收尾：将 Cube 偶/奇列分乘结果 [16,128]×4 重排为平面 mat_c [64,128]。
 * 仅 DataCopy 按行搬运，无 Gather。
 */
class AivPackMatCPlanar {
public:
    __aicore__ inline AivPackMatCPlanar(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx(subCoreIdx), coeffN(coeffN), halfLen(coeffN / 2),
          polyStart(static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kPolysPerAiv)),
          kPolysLocal(static_cast<uint16_t>(tiling::kPolysPerAiv))
    {
    }

    __aicore__ inline void Init(GM_ADDR matPlanar, GM_ADDR tmpLoEven, GM_ADDR tmpLoOdd, GM_ADDR tmpHiEven,
                                GM_ADDR tmpHiOdd)
    {
        gm_out.SetGlobalBuffer((__gm__ int32_t *)matPlanar);
        gm_lo_even.SetGlobalBuffer((__gm__ int32_t *)tmpLoEven);
        gm_lo_odd.SetGlobalBuffer((__gm__ int32_t *)tmpLoOdd);
        gm_hi_even.SetGlobalBuffer((__gm__ int32_t *)tmpHiEven);
        gm_hi_odd.SetGlobalBuffer((__gm__ int32_t *)tmpHiOdd);
        pipe.InitBuffer(que_row, 1, halfLen * sizeof(int32_t));
    }

    __aicore__ inline void copy_limb(uint32_t srcRow, AscendC::GlobalTensor<int32_t> &gmSrc, uint32_t polyIdx,
                                     uint32_t limb, uint32_t half)
    {
        const uint32_t dstRow = planar::mat_row(polyIdx, limb, half);
        LocalTensor<int32_t> row = que_row.AllocTensor<int32_t>();
        DataCopy(row, gmSrc[srcRow * halfLen], halfLen);
        KYBER_PIPE_ALL();
        DataCopy(gm_out[dstRow * halfLen], row, halfLen);
        KYBER_PIPE_ALL();
        que_row.FreeTensor(row);
    }

    __aicore__ inline void Process()
    {
        const uint32_t sub = static_cast<uint32_t>(subCoreIdx);
        for (uint16_t lp = 0; lp < kPolysLocal; ++lp) {
            const uint32_t polyIdx = static_cast<uint32_t>(polyStart) + static_cast<uint32_t>(lp);
            const uint32_t hiR = planar::s0_hi_row(sub, static_cast<uint32_t>(lp));
            const uint32_t loR = planar::s0_lo_row(sub, static_cast<uint32_t>(lp));
            copy_limb(hiR, gm_lo_even, polyIdx, 0U, 0U);
            copy_limb(hiR, gm_lo_odd, polyIdx, 1U, 0U);
            copy_limb(loR, gm_lo_even, polyIdx, 2U, 0U);
            copy_limb(loR, gm_lo_odd, polyIdx, 3U, 0U);
            copy_limb(hiR, gm_hi_even, polyIdx, 0U, 1U);
            copy_limb(hiR, gm_hi_odd, polyIdx, 1U, 1U);
            copy_limb(loR, gm_hi_even, polyIdx, 2U, 1U);
            copy_limb(loR, gm_hi_odd, polyIdx, 3U, 1U);
        }
    }

private:
    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint32_t halfLen;
    const uint16_t polyStart;
    const uint16_t kPolysLocal;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_row;
    AscendC::GlobalTensor<int32_t> gm_out;
    AscendC::GlobalTensor<int32_t> gm_lo_even, gm_lo_odd, gm_hi_even, gm_hi_odd;
};

#endif
