#ifndef __AIV_FUNC_HPP__
#define __AIV_FUNC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "ntt_vec.hpp"

#ifdef ASCENDC_CPU_DEBUG
template <typename T>
__aicore__ inline void _AivGmProbe(GM_ADDR addr, uint32_t tileLength)
{
    AscendC::TPipe pipe;
    AscendC::GlobalTensor<T> gm;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQueue;
    constexpr int32_t line = (sizeof(T) == 1) ? 32 : 16;

    pipe.InitBuffer(inQueue, 1, tileLength * sizeof(T));
    LocalTensor<T> local = inQueue.AllocTensor<T>();
    gm.SetGlobalBuffer((__gm__ T *)addr);
    AscendC::DataCopy<T>(local, gm, tileLength);
    inQueue.FreeTensor(local);
}
#define AivGmProbe(addr, T, tileLength)
#else
#define AivGmProbe(addr, T, tileLength)
#endif

using AscendC::DataCopy;

class AivSplit {
public:
    __aicore__ inline AivSplit(int32_t subCoreIdx, uint32_t tileLength) : subCoreIdx(subCoreIdx), tileLength(tileLength)
    {
    }

    __aicore__ inline void Init(GM_ADDR dst0, GM_ADDR dst1, GM_ADDR dst2, GM_ADDR dst3, GM_ADDR src)
    {
        (void)dst2;
        (void)dst3;
        gm_src.SetGlobalBuffer((__gm__ int32_t *)src);
        gm_dst0.SetGlobalBuffer((__gm__ int8_t *)dst0);
        gm_dst1.SetGlobalBuffer((__gm__ int8_t *)dst1);
        pipe.InitBuffer(in_src, 1, tileLength * sizeof(int32_t));
        pipe.InitBuffer(out_dst0, 1, tileLength * sizeof(int8_t));
        pipe.InitBuffer(out_dst1, 1, tileLength * sizeof(int8_t));
    }

    __aicore__ inline void CopyIn()
    {
        LocalTensor<int32_t> local_src = in_src.AllocTensor<int32_t>();
        AscendC::DataCopy(local_src, gm_src, tileLength);
        in_src.EnQue(local_src);
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void Compute()
    {
        LocalTensor<int32_t> local_src = in_src.DeQue<int32_t>();
        LocalTensor<int8_t> local_dst0 = out_dst0.AllocTensor<int8_t>();
        LocalTensor<int8_t> local_dst1 = out_dst1.AllocTensor<int8_t>();
        Tensor_int8x4 res{local_dst0, local_dst1};
        split_vec(res, local_src, tileLength);
        out_dst0.EnQue<int8_t>(local_dst0);
        out_dst1.EnQue<int8_t>(local_dst1);
        in_src.FreeTensor(local_src);
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void CopyOut()
    {
        LocalTensor<int8_t> local_dst0 = out_dst0.DeQue<int8_t>();
        LocalTensor<int8_t> local_dst1 = out_dst1.DeQue<int8_t>();
        AscendC::DataCopy(gm_dst0, local_dst0, tileLength);
        AscendC::DataCopy(gm_dst1, local_dst1, tileLength);
        out_dst0.FreeTensor(local_dst0);
        out_dst1.FreeTensor(local_dst1);
        KYBER_PIPE_ALL();
    }

private:
    const int32_t subCoreIdx;
    const uint32_t tileLength;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> in_src;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst0, out_dst1;
    AscendC::GlobalTensor<int32_t> gm_src;
    AscendC::GlobalTensor<int8_t> gm_dst0, gm_dst1;
};

class AivMerge {
public:
    __aicore__ inline AivMerge(int32_t subCoreIdx, uint32_t n, int32_t q) : subCoreIdx(subCoreIdx), n(n), q(q) {}

    __aicore__ inline void Init(GM_ADDR dst, GM_ADDR src0, GM_ADDR src1, GM_ADDR src2, GM_ADDR src3)
    {
        (void)src2;
        (void)src3;
        gm_src0.SetGlobalBuffer((__gm__ int32_t *)src0);
        gm_src1.SetGlobalBuffer((__gm__ int32_t *)src1);
        gm_dst.SetGlobalBuffer((__gm__ int32_t *)dst);
        pipe.InitBuffer(in_src0, 1, n * 4 * sizeof(int32_t));
        pipe.InitBuffer(in_src1, 1, n * 4 * sizeof(int32_t));
        pipe.InitBuffer(out_dst, 1, n * sizeof(int32_t));
    }

    __aicore__ inline void CopyIn()
    {
        LocalTensor<int32_t> local_src0 = in_src0.AllocTensor<int32_t>();
        LocalTensor<int32_t> local_src1 = in_src1.AllocTensor<int32_t>();

        for (int i = 0; i < 4; i++) {
            DataCopy(local_src0[i * n / 2], gm_src0[subCoreIdx * n / 2 + i * n], n / 2);
            DataCopy(local_src1[i * n / 2], gm_src1[subCoreIdx * n / 2 + i * n], n / 2);
            KYBER_PIPE_ALL();
        }

        in_src0.EnQue(local_src0);
        in_src1.EnQue(local_src1);
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void Compute()
    {
        LocalTensor<int32_t> local_src[2] = {in_src0.DeQue<int32_t>(), in_src1.DeQue<int32_t>()};
        LocalTensor<int32_t> local_dst = out_dst.AllocTensor<int32_t>();

        LocalTensor<int32_t> x[2][4];
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 4; j++) {
                x[i][j] = local_src[i][j * n / 2];
            }
        }
        using AscendC::Add;
        using AscendC::ShiftLeft;

        ShiftLeft(x[0][1], x[0][1], kKyberMergeShift1, n / 2);
        KYBER_PIPE_ALL();
        ShiftLeft(x[1][0], x[1][0], kKyberMergeShift1, n / 2);
        KYBER_PIPE_ALL();
        ShiftLeft(x[0][2], x[0][2], kKyberMergeShift2, n / 2);
        KYBER_PIPE_ALL();
        ShiftLeft(x[1][1], x[1][1], kKyberMergeShift2, n / 2);
        KYBER_PIPE_ALL();
        ShiftLeft(x[0][3], x[0][3], kKyberMergeShift3, n / 2);
        KYBER_PIPE_ALL();
        ShiftLeft(x[1][2], x[1][2], kKyberMergeShift3, n / 2);
        KYBER_PIPE_ALL();

        Add(local_dst, x[0][0], x[0][1], n / 2);
        KYBER_PIPE_ALL();
        Add(local_dst, local_dst, x[1][0], n / 2);
        KYBER_PIPE_ALL();
        Add(local_dst, local_dst, x[0][2], n / 2);
        KYBER_PIPE_ALL();
        Add(local_dst, local_dst, x[1][1], n / 2);
        KYBER_PIPE_ALL();
        Add(local_dst, local_dst, x[0][3], n / 2);
        KYBER_PIPE_ALL();
        Add(local_dst, local_dst, x[1][2], n / 2);
        KYBER_PIPE_ALL();

        barrett_mul_vec_runtime(local_dst, q, 12, 5039, local_src[0], local_src[1], n / 2);
        KYBER_PIPE_ALL();
        barrett_mul_vec_runtime(local_dst, q, 12, 5039, local_src[0], local_src[1], n / 2);
        KYBER_PIPE_ALL();

        out_dst.EnQue<int32_t>(local_dst);
        in_src0.FreeTensor(local_src[0]);
        in_src1.FreeTensor(local_src[1]);
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void CopyOut()
    {
        LocalTensor<int32_t> local_dst = out_dst.DeQue<int32_t>();
        AscendC::DataCopy(gm_dst, local_dst, n / 2);
        out_dst.FreeTensor(local_dst);
        KYBER_PIPE_ALL();
    }

private:
    const int32_t subCoreIdx;
    const uint32_t n, q;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> in_src0, in_src1;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst;
    AscendC::GlobalTensor<int32_t> gm_src0, gm_src1;
    AscendC::GlobalTensor<int32_t> gm_dst;
};

#endif
