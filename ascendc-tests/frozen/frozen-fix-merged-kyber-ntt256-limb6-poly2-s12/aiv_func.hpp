#ifndef __AIV_FUNC_HPP__
#define __AIV_FUNC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "ntt_vec.hpp"

using AscendC::DataCopy;

/**
 * Stage1 batch Split：全局一个 TPipe，一次处理 kPolys 条 poly（每 AIV 半行）。
 * tileLength = kPolys * (n/2)；GM 上逐 poly CopyIn/CopyOut，Compute 一趟 split_vec。
 */
class AivSplit {
public:
    __aicore__ inline AivSplit(int32_t subCoreIdx, uint32_t coeffN, uint16_t kPolys)
        : subCoreIdx(subCoreIdx), coeffN(coeffN), kPolys(kPolys), halfLen(coeffN / 2),
          tileLength(static_cast<uint32_t>(kPolys) * (coeffN / 2))
    {
    }

    __aicore__ inline void Init(GM_ADDR wsS0, GM_ADDR src, GM_ADDR wsS2, GM_ADDR wsS3)
    {
        (void)wsS2;
        (void)wsS3;
        gm_src.SetGlobalBuffer((__gm__ int32_t *)src);
        gm_s0.SetGlobalBuffer((__gm__ int8_t *)wsS0);
        pipe.InitBuffer(in_src, 1, tileLength * sizeof(int32_t));
        pipe.InitBuffer(out_dst0, 1, tileLength * sizeof(int8_t));
        pipe.InitBuffer(out_dst1, 1, tileLength * sizeof(int8_t));
    }

    __aicore__ inline void CopyIn()
    {
        LocalTensor<int32_t> local_src = in_src.AllocTensor<int32_t>();
        for (uint16_t p = 0; p < kPolys; p++) {
            const uint32_t srcOff = static_cast<uint32_t>(p) * coeffN + static_cast<uint32_t>(subCoreIdx) * halfLen;
            DataCopy(local_src[static_cast<uint32_t>(p) * halfLen], gm_src[srcOff], halfLen);
            KYBER_PIPE_ALL();
        }
        in_src.EnQue(local_src);
        KYBER_PIPE_ALL();
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
        for (uint16_t p = 0; p < kPolys; p++) {
            const uint32_t hiOff = static_cast<uint32_t>(2 * p) * coeffN + static_cast<uint32_t>(subCoreIdx) * halfLen;
            const uint32_t loOff = static_cast<uint32_t>(2 * p + 1) * coeffN + static_cast<uint32_t>(subCoreIdx) * halfLen;
            const uint32_t locOff = static_cast<uint32_t>(p) * halfLen;
            DataCopy(gm_s0[hiOff], local_dst0[locOff], halfLen);
            DataCopy(gm_s0[loOff], local_dst1[locOff], halfLen);
            KYBER_PIPE_ALL();
        }
        out_dst0.FreeTensor(local_dst0);
        out_dst1.FreeTensor(local_dst1);
        KYBER_PIPE_ALL();
    }

private:
    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint16_t kPolys;
    const uint32_t halfLen;
    const uint32_t tileLength;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> in_src;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst0, out_dst1;
    AscendC::GlobalTensor<int32_t> gm_src;
    AscendC::GlobalTensor<int8_t> gm_s0;
};

#endif
