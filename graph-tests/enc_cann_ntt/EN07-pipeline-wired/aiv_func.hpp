/**
 * EN01-kem256-ntt-port · AIV Split / Merge（含 Kyber 2-digit）
 * 迁入来源：thirdparty/cann-ntt-author-merged_dsa/aiv_func.hpp
 * q=3329：AivSplit2 拆 7-bit digit；AivMergeKyber 做 Barrett 合并回系数。
 */
#ifndef __AIV_FUNC_HPP__
#define __AIV_FUNC_HPP__
#include "kernel_operator.h"
#include "basic.hpp"
#include "ntt_vec.hpp"
#include <vector>

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
    // MTE2→S：DataCopy 后须经 Que 同步再 Scalar GetValue（sync_audit SYNC-02）
    AscendC::DataCopy<T>(local, gm, tileLength);
    inQueue.EnQue(local);
    local = inQueue.DeQue<T>();
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
    __aicore__ inline AivSplit(int32_t subCoreIdx, uint32_t n, int32_t bench) :
        subCoreIdx(subCoreIdx),
        n(n),
        bench(bench)
    {}
    
    __aicore__ inline void Init(GM_ADDR dst0, GM_ADDR dst1, GM_ADDR dst2, GM_ADDR dst3, GM_ADDR src) {
        // __assertion_info("SPLIT AIV Core #%d", subCoreIdx);

        gm_src.SetGlobalBuffer((__gm__ int32_t *)src);
        gm_dst0.SetGlobalBuffer((__gm__ int8_t *) dst0);
        gm_dst1.SetGlobalBuffer((__gm__ int8_t *) dst1);
        gm_dst2.SetGlobalBuffer((__gm__ int8_t *) dst2);
        gm_dst3.SetGlobalBuffer((__gm__ int8_t *) dst3);
        pipe.InitBuffer(in_src, 1, n / 2 * bench * sizeof(int32_t));
        pipe.InitBuffer(out_dst0, 1, n / 2 * bench * sizeof(int8_t));
        pipe.InitBuffer(out_dst1, 1, n / 2 * bench * sizeof(int8_t));
        pipe.InitBuffer(out_dst2, 1, n / 2 * bench * sizeof(int8_t));
        pipe.InitBuffer(out_dst3, 1, n / 2 * bench * sizeof(int8_t));
        pipe.InitBuffer(Buf1, n / 2 * bench * sizeof(int32_t));
		pipe.InitBuffer(Buf2, n / 2 * bench * sizeof(half));
    }

    __aicore__ inline void CopyIn() {
        LocalTensor<int32_t> local_src = in_src.AllocTensor<int32_t>();
        AscendC::DataCopy(local_src, gm_src[subCoreIdx * n * bench / 2], n * bench / 2);
        in_src.EnQue(local_src);
    }

    __aicore__ inline void Compute() {
        const int32_t tileLength = n / 2 * bench;
        LocalTensor<int32_t> local_src = in_src.DeQue<int32_t>();
        LocalTensor<int8_t> local_dst0 = out_dst0.AllocTensor<int8_t>();
        LocalTensor<int8_t> local_dst1 = out_dst1.AllocTensor<int8_t>();
        LocalTensor<int8_t> local_dst2 = out_dst2.AllocTensor<int8_t>();
        LocalTensor<int8_t> local_dst3 = out_dst3.AllocTensor<int8_t>();
        LocalTensor<int32_t> t1 = Buf1.Get<int32_t>();
        LocalTensor<half>    t2 = Buf2.Get<half>();
        Tensor_int8x4 res{local_dst0, local_dst1, local_dst2, local_dst3};

        // __print_tensor_short(local_src, 256, 32);
        split_vec_int7x4(res, local_src, t1, t2, tileLength);

        out_dst0.EnQue<int8_t>(local_dst0);
        out_dst1.EnQue<int8_t>(local_dst1);
        out_dst2.EnQue<int8_t>(local_dst2);
        out_dst3.EnQue<int8_t>(local_dst3);
        in_src.FreeTensor(local_src);
    }

    __aicore__ inline void CopyOut() {
        LocalTensor<int8_t> local_dst0 = out_dst0.DeQue<int8_t>();
        LocalTensor<int8_t> local_dst1 = out_dst1.DeQue<int8_t>();
        LocalTensor<int8_t> local_dst2 = out_dst2.DeQue<int8_t>();
        LocalTensor<int8_t> local_dst3 = out_dst3.DeQue<int8_t>();
        AscendC::DataCopy(gm_dst0[subCoreIdx * n * bench / 2], local_dst0, n * bench / 2);
        AscendC::DataCopy(gm_dst1[subCoreIdx * n * bench / 2], local_dst1, n * bench / 2);
        AscendC::DataCopy(gm_dst2[subCoreIdx * n * bench / 2], local_dst2, n * bench / 2);
        AscendC::DataCopy(gm_dst3[subCoreIdx * n * bench / 2], local_dst3, n * bench / 2);
        out_dst0.FreeTensor(local_dst0);
        out_dst1.FreeTensor(local_dst1);
        out_dst2.FreeTensor(local_dst2);
        out_dst3.FreeTensor(local_dst3);
    }


private:
    const int32_t subCoreIdx, bench;
    const uint32_t n; 
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> Buf1, Buf2;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> in_src;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst0, out_dst1, out_dst2, out_dst3;

    AscendC::GlobalTensor<int32_t> gm_src;
    AscendC::GlobalTensor<int8_t> gm_dst0, gm_dst1, gm_dst2, gm_dst3;
};

class AivSplit2 {
public:
    __aicore__ inline AivSplit2(int32_t subCoreIdx, uint32_t n, int32_t bench) :
        subCoreIdx(subCoreIdx),
        n(n),
        bench(bench)
    {}

    __aicore__ inline void Init(GM_ADDR dst0, GM_ADDR dst1, GM_ADDR src) {
        gm_src.SetGlobalBuffer((__gm__ int32_t *)src);
        gm_dst0.SetGlobalBuffer((__gm__ int8_t *) dst0);
        gm_dst1.SetGlobalBuffer((__gm__ int8_t *) dst1);
        pipe.InitBuffer(in_src, 1, n / 2 * bench * sizeof(int32_t));
        pipe.InitBuffer(out_dst0, 1, n / 2 * bench * sizeof(int8_t));
        pipe.InitBuffer(out_dst1, 1, n / 2 * bench * sizeof(int8_t));
        pipe.InitBuffer(Buf1, n / 2 * bench * sizeof(int32_t));
        pipe.InitBuffer(Buf2, n / 2 * bench * sizeof(half));
    }

    __aicore__ inline void CopyIn() {
        LocalTensor<int32_t> local_src = in_src.AllocTensor<int32_t>();
        AscendC::DataCopy(local_src, gm_src[subCoreIdx * n * bench / 2], n * bench / 2);
        in_src.EnQue(local_src);
    }

    __aicore__ inline void Compute() {
        const int32_t tileLength = n / 2 * bench;
        LocalTensor<int32_t> local_src = in_src.DeQue<int32_t>();
        LocalTensor<int8_t> local_dst0 = out_dst0.AllocTensor<int8_t>();
        LocalTensor<int8_t> local_dst1 = out_dst1.AllocTensor<int8_t>();
        LocalTensor<int32_t> t1 = Buf1.Get<int32_t>();
        LocalTensor<half> t2 = Buf2.Get<half>();
        Tensor_int8x2 res{local_dst0, local_dst1};

        split_vec_int7x2(res, local_src, t1, t2, tileLength);

        out_dst0.EnQue<int8_t>(local_dst0);
        out_dst1.EnQue<int8_t>(local_dst1);
        in_src.FreeTensor(local_src);
    }

    __aicore__ inline void CopyOut() {
        LocalTensor<int8_t> local_dst0 = out_dst0.DeQue<int8_t>();
        LocalTensor<int8_t> local_dst1 = out_dst1.DeQue<int8_t>();
        AscendC::DataCopy(gm_dst0[subCoreIdx * n * bench / 2], local_dst0, n * bench / 2);
        AscendC::DataCopy(gm_dst1[subCoreIdx * n * bench / 2], local_dst1, n * bench / 2);
        out_dst0.FreeTensor(local_dst0);
        out_dst1.FreeTensor(local_dst1);
    }

private:
    const int32_t subCoreIdx, bench;
    const uint32_t n;
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> Buf1, Buf2;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> in_src;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst0, out_dst1;

    AscendC::GlobalTensor<int32_t> gm_src;
    AscendC::GlobalTensor<int8_t> gm_dst0, gm_dst1;
};


class AivMergeKyber {
public:
    __aicore__ inline AivMergeKyber(int32_t subCoreIdx, uint32_t n, int32_t bench) :
        subCoreIdx(subCoreIdx), bench(bench), n(n)
    {}

    __aicore__ inline void Init(GM_ADDR dst, GM_ADDR src0, GM_ADDR src1) {
        gm_src0.SetGlobalBuffer((__gm__ int32_t *)src0);
        gm_src1.SetGlobalBuffer((__gm__ int32_t *)src1);
        gm_dst.SetGlobalBuffer((__gm__ int32_t *)dst);

        pipe.InitBuffer(in_src0, 1, 2 * MaxLocalCount() * sizeof(int32_t));
        pipe.InitBuffer(in_src1, 1, 2 * MaxLocalCount() * sizeof(int32_t));
        pipe.InitBuffer(out_dst, 1, MaxLocalCount() * sizeof(int32_t));
    }

    __aicore__ inline void Process() {
        const int32_t halfBench = bench >> 1;
        for (int32_t rowBase = 0; rowBase < halfBench; rowBase += kMergeRows) {
            const int32_t rows = (halfBench - rowBase) < kMergeRows
                               ? (halfBench - rowBase)
                               : kMergeRows;
            const int32_t count = rows * (int32_t)n;
            const int32_t rowOffset = subCoreIdx * halfBench + rowBase;

            CopyIn(rowOffset, count);
            Compute(count);
            CopyOut(rowOffset, count);
        }
    }

private:
    static constexpr int32_t kMergeRows = 16;
    static constexpr int32_t kKyberBias = 3329 * 16384;

    __aicore__ inline int32_t MaxLocalCount() const {
        return kMergeRows * (int32_t)n;
    }

    __aicore__ inline void CopyIn(const int32_t rowOffset, const int32_t count) {
        const int32_t maxCount = MaxLocalCount();
        LocalTensor<int32_t> local_src0 = in_src0.AllocTensor<int32_t>();
        LocalTensor<int32_t> local_src1 = in_src1.AllocTensor<int32_t>();

        DataCopy(local_src0, gm_src0[rowOffset * (int32_t)n], count);
        DataCopy(local_src0[maxCount], gm_src0[(bench + rowOffset) * (int32_t)n], count);
        DataCopy(local_src1, gm_src1[rowOffset * (int32_t)n], count);
        DataCopy(local_src1[maxCount], gm_src1[(bench + rowOffset) * (int32_t)n], count);

        in_src0.EnQue(local_src0);
        in_src1.EnQue(local_src1);
    }

    __aicore__ inline void Compute(const int32_t count) {
        using AscendC::Add, AscendC::Adds, AscendC::Muls, AscendC::Sub;
        const int32_t maxCount = MaxLocalCount();

        LocalTensor<int32_t> local_src0 = in_src0.DeQue<int32_t>();
        LocalTensor<int32_t> local_src1 = in_src1.DeQue<int32_t>();
        LocalTensor<int32_t> local_dst = out_dst.AllocTensor<int32_t>();

        LocalTensor<int32_t> x00 = local_src0;
        LocalTensor<int32_t> x10 = local_src0[maxCount];
        LocalTensor<int32_t> x01 = local_src1;
        LocalTensor<int32_t> x11 = local_src1[maxCount];

        Add(local_dst, x01, x10, count);
        Muls(local_dst, local_dst, 128, count);
        Add(local_dst, local_dst, x00, count);
        Muls(x11, x11, 261, count);
        Sub(local_dst, local_dst, x11, count);
        Adds(local_dst, local_dst, kKyberBias, count);
        kyber_barrett_reduce_twice_vec(local_dst, x01, x10, count);

        out_dst.EnQue<int32_t>(local_dst);
        in_src0.FreeTensor(local_src0);
        in_src1.FreeTensor(local_src1);
    }

    __aicore__ inline void CopyOut(const int32_t rowOffset, const int32_t count) {
        LocalTensor<int32_t> local_dst = out_dst.DeQue<int32_t>();
        DataCopy(gm_dst[rowOffset * (int32_t)n], local_dst, count);
        out_dst.FreeTensor(local_dst);
    }

    const int32_t subCoreIdx, bench;
    const uint32_t n;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> in_src0, in_src1;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst;
    AscendC::GlobalTensor<int32_t> gm_src0, gm_src1;
    AscendC::GlobalTensor<int32_t> gm_dst;
};


class AivMerge {
    public:
        __aicore__ inline AivMerge(int32_t subCoreIdx, uint32_t n, int32_t bench,
                                   int32_t q, int32_t b_k, int32_t b_mu, int32_t r28) :
            subCoreIdx(subCoreIdx),
            bench(bench), q(q), b_k(b_k), b_mu(b_mu), r28(r28), n(n)
        {}
        
        __aicore__ inline void Init(GM_ADDR dst, GM_ADDR src0, GM_ADDR src1, GM_ADDR src2, GM_ADDR src3) {
            // __assertion_info("MERGE AIV Core #%d", subCoreIdx);
    
            gm_src0.SetGlobalBuffer((__gm__ int32_t *)src0);
            gm_src1.SetGlobalBuffer((__gm__ int32_t *)src1);
            gm_src2.SetGlobalBuffer((__gm__ int32_t *)src2);
            gm_src3.SetGlobalBuffer((__gm__ int32_t *)src3);
            gm_dst.SetGlobalBuffer((__gm__ int32_t *) dst);
            if (q == 3329) {
                pipe.InitBuffer(in_src0, 1, n * 2 * bench * sizeof(int32_t));
                pipe.InitBuffer(in_src1, 1, n * 2 * bench * sizeof(int32_t));
            } else {
                pipe.InitBuffer(in_src0, 1, n * 4 * bench * sizeof(int32_t));
                pipe.InitBuffer(in_src1, 1, n * 4 * bench * sizeof(int32_t));
                pipe.InitBuffer(in_src2, 1, n * 4 * bench * sizeof(int32_t));
                pipe.InitBuffer(in_src3, 1, n * 4 * bench * sizeof(int32_t));
            }
            pipe.InitBuffer(out_dst, 1, n / 2 * bench * sizeof(int32_t));
        }
    
        __aicore__ inline void CopyIn() {
            LocalTensor<int32_t> local_src0 = in_src0.AllocTensor<int32_t>();
            LocalTensor<int32_t> local_src1 = in_src1.AllocTensor<int32_t>();
            if (q == 3329) {
                for(int i = 0; i < 2; i++) {
                    DataCopy(local_src0[i * n * bench / 2], gm_src0[subCoreIdx * n * bench / 2 + i * n * bench], n * bench / 2);
                    DataCopy(local_src1[i * n * bench / 2], gm_src1[subCoreIdx * n * bench / 2 + i * n * bench], n * bench / 2);
                }
                in_src0.EnQue(local_src0);
                in_src1.EnQue(local_src1);
                return;
            }
            LocalTensor<int32_t> local_src2 = in_src2.AllocTensor<int32_t>();
            LocalTensor<int32_t> local_src3 = in_src3.AllocTensor<int32_t>();
            for(int i = 0; i < 4; i++) {
                DataCopy(local_src0[i * n * bench / 2], gm_src0[subCoreIdx * n * bench / 2 + i * n * bench], n * bench / 2);
                DataCopy(local_src1[i * n * bench / 2], gm_src1[subCoreIdx * n * bench / 2 + i * n * bench], n * bench / 2);
                DataCopy(local_src2[i * n * bench / 2], gm_src2[subCoreIdx * n * bench / 2 + i * n * bench], n * bench / 2);
                DataCopy(local_src3[i * n * bench / 2], gm_src3[subCoreIdx * n * bench / 2 + i * n * bench], n * bench / 2);
            }

            in_src0.EnQue(local_src0);
            in_src1.EnQue(local_src1);
            in_src2.EnQue(local_src2);
            in_src3.EnQue(local_src3);
        }
    
        __aicore__ inline void ShiftRightU(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &src, int32_t scalar, const int32_t count) {
            AscendC::ShiftRight(tr<uint32_t>(dst), tr<uint32_t>(src), (uint32_t)scalar, count);
        }

        __aicore__ inline void Compute() {
            const int32_t count = n * bench / 2;
            using AscendC::ShiftLeft, AscendC::Add, AscendC::ShiftRight, AscendC::Muls;
            if (q == 3329) {
                LocalTensor<int32_t> local_src0 = in_src0.DeQue<int32_t>();
                LocalTensor<int32_t> local_src1 = in_src1.DeQue<int32_t>();
                LocalTensor<int32_t> local_dst = out_dst.AllocTensor<int32_t>();

                LocalTensor<int32_t> x00 = local_src0;
                LocalTensor<int32_t> x01 = local_src0[n * bench / 2];
                LocalTensor<int32_t> x10 = local_src1;
                LocalTensor<int32_t> x11 = local_src1[n * bench / 2];

                // Kyber q=3329 uses only two 7-bit digits:
                //   m = m0 + 128*m1, x = x0 + 128*x1
                // MMAD gives:
                //   x00 = x0*M0, x01 = x0*M1, x10 = x1*M0, x11 = x1*M1
                // Combine as:
                //   y = x00 + 128*(x01+x10) + (128*128 mod 3329)*x11
                //     = x00 + 128*(x01+x10) + 3068*x11  (mod 3329)
                //
                // The previous code reduced after almost every term. For Kyber the
                // partial ranges are still int32-safe, so only x11 needs reduction
                // before multiplying by 3068. The final accumulator is reduced twice
                // because barrett_mul_vec_runtime() performs only one corrective
                // subtract and its single pass is not exact for ~1G inputs.
                Add(local_dst, x01, x10, count);          // x01 + x10
                Muls(local_dst, local_dst, 128, count);   // 128*(x01+x10), int32-safe
                Add(local_dst, local_dst, x00, count);    // + x00

                barrett_mul_vec_runtime(x11, q, b_k, b_mu, x01, x10, count);
                Muls(x11, x11, 3068, count);              // 128^2 mod 3329 = 3068
                Add(local_dst, local_dst, x11, count);

                barrett_mul_vec_runtime(local_dst, q, b_k, b_mu, x01, x10, count);
                barrett_mul_vec_runtime(local_dst, q, b_k, b_mu, x01, x10, count);

                out_dst.EnQue<int32_t>(local_dst);
                in_src0.FreeTensor(local_src0);
                in_src1.FreeTensor(local_src1);
                return;
            }

            LocalTensor<int32_t> local_src[4] = {
                in_src0.DeQue<int32_t>(),
                in_src1.DeQue<int32_t>(),
                in_src2.DeQue<int32_t>(),
                in_src3.DeQue<int32_t>()
            };
            LocalTensor<int32_t> local_dst = out_dst.AllocTensor<int32_t>();

            LocalTensor<int32_t> x[4][4];
            for(int i = 0; i < 4; i++) {
                for(int j = 0; j < 4; j++) {
                    x[i][j] = local_src[i][j * n * bench / 2];
                }
            }
            // 高 28 位，储存在 x[3][3]

            #ifdef ASCENDC_CPU_DEBUG
            std::vector<int64_t> tx(count), ty(count);
            for (int i = 0; i < count; i++) {
                tx[i]  = x[0][0].GetValue(i);
                tx[i] += int64_t(x[0][1].GetValue(i) + x[1][0].GetValue(i)) << 7;
                tx[i] += int64_t(x[0][2].GetValue(i) + x[1][1].GetValue(i) + x[2][0].GetValue(i)) << 14;
                tx[i] += int64_t(x[0][3].GetValue(i) + x[1][2].GetValue(i) + x[2][1].GetValue(i) + x[3][0].GetValue(i)) << 21;
                tx[i] += int64_t(x[1][3].GetValue(i) + x[2][2].GetValue(i) + x[3][1].GetValue(i)) << 28;
                tx[i] += int64_t(x[2][3].GetValue(i) + x[3][2].GetValue(i)) << 35;
                tx[i] += int64_t(x[3][3].GetValue(i)) << 42;
            }
            #endif
            Add(x[0][1], x[0][1], x[1][0], count);
            Add(x[0][2], x[0][2], x[1][1], count);
            Add(x[0][2], x[0][2], x[2][0], count);
            Add(x[0][3], x[0][3], x[1][2], count);
            Add(x[0][3], x[0][3], x[2][1], count);
            Add(x[0][3], x[0][3], x[3][0], count);
            Add(x[1][3], x[1][3], x[2][2], count);
            Add(x[1][3], x[1][3], x[3][1], count);
            Add(x[2][3], x[2][3], x[3][2], count);
            // 计算低 28 位 -> a
            auto &a = x[1][0], &b = x[2][1], &t1 = x[2][0];
            ShiftLeft(a,  x[0][0], 0 + 4, count);
            ShiftLeft(t1, x[0][1], 7 + 4, count);
            Add(a, a, t1, count);
            ShiftLeft(t1, x[0][2], 14 + 4, count);
            Add(a, a, t1, count);
            ShiftLeft(t1, x[0][3], 21 + 4, count);
            Add(a, a, t1, count);
            ShiftRightU(a, a, 4, count);

            // 计算高位，特别：22位 -> t1
            ShiftRightU(t1, x[0][0], 7, count);
            Add(t1, t1, x[0][1], count);
            ShiftRightU(t1, t1, 7, count);
            Add(t1, t1, x[0][2], count);
            ShiftRightU(t1, t1, 7, count);
            Add(t1, t1, x[0][3], count);
            ShiftRightU(b, t1, 1, count); // 已经达到 22 位高
            ShiftLeft(t1, x[1][3], 28 - 22, count);
            Add(b, b, t1, count);
            ShiftLeft(t1, x[2][3], 35 - 22, count);
            Add(b, b, t1, count);
            ShiftLeft(t1, x[3][3], 42 - 22, count);
            Add(t1, b, t1, count);

            #ifdef ASCENDC_CPU_DEBUG
            std::vector<int64_t> ta(count), tb(count);
            for (int i = 0; i < count; i++) {
                ta[i] = tx[i] & ((1L << 28) - 1);
                tb[i] = tx[i] >> 22;
            }
            __check_diff(a, ta, count);
            __check_diff(t1, tb, count);
            #endif

            auto &t1_lo = x[2][2]; auto &t1_hi = x[2][3];
            if (q == 8380417) {
                ShiftRight(t1_hi, t1, 12, count);
                ShiftLeft(t1_lo, t1_hi, 12, count);
                Sub(t1_lo, t1, t1_lo, count);

                const int32_t mu_hi = b_mu >> 12;
                const int32_t mu_lo = b_mu & 0xfff;
                Muls(t1_lo, t1_lo, mu_hi, count);
                Muls(t1, t1_hi, mu_lo, count);
                Add(t1, t1, t1_lo, count);
                ShiftRightU(t1, t1, 12, count);
                Muls(t1_hi, t1_hi, mu_hi, count);
                Add(t1, t1, t1_hi, count);

                Muls(t1, t1, q, count);
                Sub(local_dst, a, t1, count);
                ShiftLeft(local_dst, local_dst, 32 - 27, count);
                ShiftRightU(local_dst, local_dst, 32 - 27, count);
                wrap_mod_vec_runtime(local_dst, local_dst, q, t1_lo, t1_hi, n * bench / 2);
            } else {
                // For small moduli such as Kyber q=3329, reduce
                // Z = low28 + high28 * 2^28 without constructing 64-bit Z.
                ShiftRightU(t1_hi, t1, 6, count);
                barrett_mul_vec_runtime(a, q, b_k, b_mu, t1_lo, t1, count);
                barrett_mul_vec_runtime(a, q, b_k, b_mu, t1_lo, t1, count);
                Muls(t1_hi, t1_hi, r28, count);
                Add(local_dst, a, t1_hi, count);
                barrett_mul_vec_runtime(local_dst, q, b_k, b_mu, t1_lo, t1, count);
            }
            out_dst.EnQue<int32_t>(local_dst);
            in_src0.FreeTensor(local_src[0]);
            in_src1.FreeTensor(local_src[1]);
            in_src2.FreeTensor(local_src[2]);
            in_src3.FreeTensor(local_src[3]);
        }
    
        __aicore__ inline void CopyOut() {
            LocalTensor<int32_t> local_dst = out_dst.DeQue<int32_t>();
            AscendC::DataCopy(gm_dst[subCoreIdx * n * bench / 2], local_dst, n * bench / 2);
            out_dst.FreeTensor(local_dst);
        }
    
    
    private:
        const int32_t subCoreIdx, bench;
        const int32_t q, b_k, b_mu, r28;
        const uint32_t n;
        AscendC::TPipe pipe;
        AscendC::TQue<AscendC::TPosition::VECIN, 1> in_src0, in_src1, in_src2, in_src3;
        AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst;
    
        AscendC::GlobalTensor<int32_t> gm_src0, gm_src1, gm_src2, gm_src3;
        AscendC::GlobalTensor<int32_t> gm_dst;
    };

#endif
