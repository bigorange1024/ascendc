#ifndef __AIV_FUNC_HPP__
#define __AIV_FUNC_HPP__
#include "kernel_operator.h"
#include "basic.hpp"
#include "ntt_vec.hpp"

#ifdef ASCENDC_CPU_DEBUG
template <typename T>
__aicore__ inline void _AivGmProbe(GM_ADDR addr, uint32_t tileLength) {
    AscendC::TPipe pipe;
    AscendC::GlobalTensor<T> gm;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQueue;
    constexpr int32_t line = (sizeof(T) == 1) ? 32 : 16;

    pipe.InitBuffer(inQueue, 1, tileLength * sizeof(T));
    LocalTensor<T> local = inQueue.AllocTensor<T>();
    gm.SetGlobalBuffer((__gm__ T *)addr);
    AscendC::DataCopy<T>(local, gm, tileLength);
    {
        for(uint32_t i=0; i<(uint32_t)tileLength; i++) {
            if constexpr(sizeof(T) > 1) {
                AscendC::printf("%8d ", local.GetValue(i));
            } else {
                AscendC::printf("%4d ", local.GetValue(i));
            }
            if((i + 1) % line == 0) AscendC::printf("\n");
        }
        AscendC::printf("\n");
    }
    inQueue.FreeTensor(local);
}
#define AivGmProbe(addr, T, tileLength) \
{AscendC::printf("[%ld]\033[1;32mgm[<%s> %s] \033[0mlen = %d, %s:%d\n", AscendC::GetBlockIdx(), #T, #addr, tileLength, __FILE__, __LINE__);\
_AivGmProbe<T>(addr, tileLength);}
#else
#define AivGmProbe(addr, T, tileLength)
#endif

using AscendC::DataCopy;

class AivSplit {
public:
    __aicore__ inline AivSplit(int32_t subCoreIdx, uint32_t tileLength) :
        subCoreIdx(subCoreIdx),
        tileLength(tileLength)
    {}
    
    /**
     * 绑定 GM 指针（可多次）；不分配 TQue。
     * 背景：E07 同核 NTT+INTT 两趟 Split，禁止二次 InitBuffer/AllocEventID。
     */
    __aicore__ inline void Bind(GM_ADDR dst0, GM_ADDR dst1, GM_ADDR dst2, GM_ADDR dst3, GM_ADDR src) {
        (void)dst2;
        (void)dst3;
        gm_src.SetGlobalBuffer((__gm__ int32_t *)src);
        gm_dst0.SetGlobalBuffer((__gm__ int8_t *)dst0);
        gm_dst1.SetGlobalBuffer((__gm__ int8_t *)dst1);
    }

    /** 首次：Bind + InitBuffer（整核生命周期仅一次）。 */
    __aicore__ inline void Init(GM_ADDR dst0, GM_ADDR dst1, GM_ADDR dst2, GM_ADDR dst3, GM_ADDR src) {
        Bind(dst0, dst1, dst2, dst3, src);
        pipe.InitBuffer(in_src, 1, tileLength * sizeof(int32_t));
        pipe.InitBuffer(out_dst0, 1, tileLength * sizeof(int8_t));
        pipe.InitBuffer(out_dst1, 1, tileLength * sizeof(int8_t));
    }

    __aicore__ inline void CopyIn() {
        LocalTensor<int32_t> local_src = in_src.AllocTensor<int32_t>();
        AscendC::DataCopy(local_src, gm_src, tileLength);
        in_src.EnQue(local_src);
    }

    __aicore__ inline void Compute() {
        LocalTensor<int32_t> local_src = in_src.DeQue<int32_t>();
        LocalTensor<int8_t> local_dst0 = out_dst0.AllocTensor<int8_t>();
        LocalTensor<int8_t> local_dst1 = out_dst1.AllocTensor<int8_t>();
        // LocalTensor<int8_t> local_dst2 = out_dst2.AllocTensor<int8_t>();
        // LocalTensor<int8_t> local_dst3 = out_dst3.AllocTensor<int8_t>();
        Tensor_int8x4 res{local_dst0, local_dst1};

        // __print_tensor_short(local_src, 256, 32);
        split_vec(res, local_src, tileLength);
        out_dst0.EnQue<int8_t>(local_dst0);
        out_dst1.EnQue<int8_t>(local_dst1);
        // out_dst2.EnQue<int8_t>(local_dst2);
        // out_dst3.EnQue<int8_t>(local_dst3);
        in_src.FreeTensor(local_src);
    }

    __aicore__ inline void CopyOut() {
        LocalTensor<int8_t> local_dst0 = out_dst0.DeQue<int8_t>();
        LocalTensor<int8_t> local_dst1 = out_dst1.DeQue<int8_t>();
        // LocalTensor<int8_t> local_dst2 = out_dst2.DeQue<int8_t>();
        // LocalTensor<int8_t> local_dst3 = out_dst3.DeQue<int8_t>();
        AscendC::DataCopy(gm_dst0, local_dst0, tileLength);
        AscendC::DataCopy(gm_dst1, local_dst1, tileLength);
        // AscendC::DataCopy(gm_dst2, local_dst2, tileLength);
        // AscendC::DataCopy(gm_dst3, local_dst3, tileLength);
        out_dst0.FreeTensor(local_dst0);
        out_dst1.FreeTensor(local_dst1);
        // out_dst2.FreeTensor(local_dst2);
        // out_dst3.FreeTensor(local_dst3);
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
        __aicore__ inline AivMerge(int32_t subCoreIdx, uint32_t n, int32_t q) :
            subCoreIdx(subCoreIdx),
            n(n), q(q)
        {}
        
        /**
         * 绑定 GM（可多次）；不分配 TQue。
         * 背景：E07 同核 NTT+INTT 两趟 Merge，禁止二次 InitBuffer。
         */
        __aicore__ inline void Bind(GM_ADDR dst, GM_ADDR src0, GM_ADDR src1, GM_ADDR src2, GM_ADDR src3) {
            (void)src2;
            (void)src3;
            gm_src0.SetGlobalBuffer((__gm__ int32_t *)src0);
            gm_src1.SetGlobalBuffer((__gm__ int32_t *)src1);
            gm_dst.SetGlobalBuffer((__gm__ int32_t *)dst);
        }

        /** 首次：Bind + InitBuffer（整核生命周期仅一次）。 */
        __aicore__ inline void Init(GM_ADDR dst, GM_ADDR src0, GM_ADDR src1, GM_ADDR src2, GM_ADDR src3) {
            Bind(dst, src0, src1, src2, src3);
            pipe.InitBuffer(in_src0, 1, n * 4 * sizeof(int32_t));
            pipe.InitBuffer(in_src1, 1, n * 4 * sizeof(int32_t));
            pipe.InitBuffer(out_dst, 1, n * sizeof(int32_t));
        }
    
        __aicore__ inline void CopyIn() {
            LocalTensor<int32_t> local_src0 = in_src0.AllocTensor<int32_t>();
            LocalTensor<int32_t> local_src1 = in_src1.AllocTensor<int32_t>();
            // LocalTensor<int32_t> local_src2 = in_src2.AllocTensor<int32_t>();
            // LocalTensor<int32_t> local_src3 = in_src3.AllocTensor<int32_t>();
            
            for(int i = 0; i < 4; i++) {
                DataCopy(local_src0[i * n / 2], gm_src0[subCoreIdx * n / 2 + i * n], n / 2);
                DataCopy(local_src1[i * n / 2], gm_src1[subCoreIdx * n / 2 + i * n], n / 2);
                // DataCopy(local_src2[i * n / 2], gm_src2[subCoreIdx * n / 2 + i * n], n / 2);
                // DataCopy(local_src3[i * n / 2], gm_src3[subCoreIdx * n / 2 + i * n], n / 2);
            }

            in_src0.EnQue(local_src0);
            in_src1.EnQue(local_src1);
            // in_src2.EnQue(local_src2);
            // in_src3.EnQue(local_src3);
        }
    
        __aicore__ inline void Compute() {
            LocalTensor<int32_t> local_src[2] = {
                in_src0.DeQue<int32_t>(),
                in_src1.DeQue<int32_t>()
            };
            LocalTensor<int32_t> local_dst = out_dst.AllocTensor<int32_t>();

            LocalTensor<int32_t> x[2][4];
            for(int i = 0; i < 2; i++) {
                for(int j = 0; j < 4; j++) {
                    x[i][j] = local_src[i][j * n / 2];
                }
            }
            using AscendC::ShiftLeft, AscendC::Add;

            ShiftLeft(x[0][1], x[0][1], 7,  n / 2);
            ShiftLeft(x[1][0], x[1][0], 7,  n / 2);
            ShiftLeft(x[0][2], x[0][2], 14, n / 2);
            ShiftLeft(x[1][1], x[1][1], 14, n / 2);
            // ShiftLeft(x[2][0], x[2][0], 14, n / 2);
            ShiftLeft(x[0][3], x[0][3], 21, n / 2);
            ShiftLeft(x[1][2], x[1][2], 21, n / 2);
            // ShiftLeft(x[2][1], x[2][1], 21, n / 2);
            // ShiftLeft(x[3][0], x[3][0], 21, n / 2);

            Add(local_dst, x[0][0],   x[0][1], n / 2);
            Add(local_dst, local_dst, x[1][0], n / 2);
            Add(local_dst, local_dst, x[0][2], n / 2);
            Add(local_dst, local_dst, x[1][1], n / 2);
            // Add(local_dst, local_dst, x[2][0], n / 2);
            Add(local_dst, local_dst, x[0][3], n / 2);
            Add(local_dst, local_dst, x[1][2], n / 2);
            // Add(local_dst, local_dst, x[2][1], n / 2);
            // Add(local_dst, local_dst, x[3][0], n / 2);

            // for(int i = 0; i < n / 2; i++) {
            //     int32_t val = local_dst.GetValue(i) % q;
            //     local_dst.SetValue(i, val);
            // }
            // b_k = 12, b_mu = 5039;

            barrett_mul_vec_runtime(local_dst, 3329, 12, 5039, local_src[0], local_src[1], n / 2);
            barrett_mul_vec_runtime(local_dst, 3329, 12, 5039, local_src[0], local_src[1], n / 2);


            out_dst.EnQue<int32_t>(local_dst);
            in_src0.FreeTensor(local_src[0]);
            in_src1.FreeTensor(local_src[1]);
            // in_src2.FreeTensor(local_src[2]);
            // in_src3.FreeTensor(local_src[3]);
        }
    
        __aicore__ inline void CopyOut() {
            LocalTensor<int32_t> local_dst = out_dst.DeQue<int32_t>();
            AscendC::DataCopy(gm_dst, local_dst, n / 2);
            out_dst.FreeTensor(local_dst);
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
