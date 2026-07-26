/**
 * @file aiv_func.hpp
 * @brief sepolyvec8 AIV：Stage1 batch Split 与 Stage3 batch Merge。
 *
 * 流水线位置：MIX 核 AIV 侧；与 AIC MMAD 经 CrossCore 握手。
 * 语义：poly-batch — 每个 AIV 握有完整 poly 的 hi+lo 半段（subCoreIdx 分半）。
 * 与 golden：仅 I/O 等价。
 */
#ifndef __AIV_FUNC_HPP__
#define __AIV_FUNC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "ntt_vec.hpp"

using AscendC::DataCopy;

/** Stage1 batch Split：单 TPipe，kPolys 条 poly 一趟 split_vec。 */
class AivSplit {
public:
    __aicore__ inline AivSplit(int32_t subCoreIdx, uint32_t coeffN, uint16_t kPolys)
        : subCoreIdx(subCoreIdx), coeffN(coeffN), kPolys(kPolys), halfLen(coeffN / 2),
          tileLength(static_cast<uint32_t>(kPolys) * (coeffN / 2))
    {
    }

    /**
     * 绑定 src 与 S0；wsS2/S3 本路径未用（保留签名）。
     * @param wsS0 Stage1 输出 A[16,256] int8；@param src se [8,256] int32
     */
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

    /** 按 poly 拉取本 AIV 半段系数到 UB（subCoreIdx*halfLen）。 */
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

    /** limb6 split_vec：写 lo/hi 两路 int8。 */
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

    /** 写回 S0：行序 [hi0,lo0,…,hi7,lo7]，每行半段按 subCoreIdx。 */
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

/**
 * Stage3 batch Merge：单 TPipe。
 * 从紧凑 A0/A1 [2*kPolys,256] 按 poly 取行 2p/2p+1（槽 2/3 置零，等价单 poly limb6）。
 */
class AivMerge {
public:
    __aicore__ inline AivMerge(int32_t subCoreIdx, uint32_t coeffN, uint16_t kPolys, int32_t q)
        : subCoreIdx(subCoreIdx), coeffN(coeffN), kPolys(kPolys), halfLen(coeffN / 2), q(q),
          polyStride(4 * (coeffN / 2)),
          outTileLength(static_cast<uint32_t>(kPolys) * (coeffN / 2))
    {
    }

    /**
     * 绑定 dst 与 A0/A1（Stage2 积）。
     * @param dst 输出 [8,256]；@param a0/@param a1 Cube 输出半幅
     */
    __aicore__ inline void Init(GM_ADDR dst, GM_ADDR a0, GM_ADDR a1)
    {
        gm_dst.SetGlobalBuffer((__gm__ int32_t *)dst);
        gm_a0.SetGlobalBuffer((__gm__ int32_t *)a0);
        gm_a1.SetGlobalBuffer((__gm__ int32_t *)a1);
        pipe.InitBuffer(in_src0, 1, static_cast<uint32_t>(kPolys) * polyStride * sizeof(int32_t));
        pipe.InitBuffer(in_src1, 1, static_cast<uint32_t>(kPolys) * polyStride * sizeof(int32_t));
        pipe.InitBuffer(out_dst, 1, outTileLength * sizeof(int32_t));
        pipe.InitBuffer(tmp_poly, 1, halfLen * sizeof(int32_t));
    }

    /** 读 A0/A1 的 hi/lo 行到 UB；槽 2/3 置零以对齐单 poly limb6 四槽布局。 */
    __aicore__ inline void CopyIn()
    {
        const uint32_t bufLen = static_cast<uint32_t>(kPolys) * polyStride;
        LocalTensor<int32_t> local_src0 = in_src0.AllocTensor<int32_t>();
        LocalTensor<int32_t> local_src1 = in_src1.AllocTensor<int32_t>();
        AscendC::Duplicate(local_src0, 0, bufLen);
        AscendC::Duplicate(local_src1, 0, bufLen);
        KYBER_PIPE_ALL();

        for (uint16_t p = 0; p < kPolys; p++) {
            const uint32_t base = static_cast<uint32_t>(p) * polyStride;
            const uint32_t rowHi = static_cast<uint32_t>(2 * p) * coeffN + static_cast<uint32_t>(subCoreIdx) * halfLen;
            const uint32_t rowLo = static_cast<uint32_t>(2 * p + 1) * coeffN + static_cast<uint32_t>(subCoreIdx) * halfLen;
            DataCopy(local_src0[base], gm_a0[rowHi], halfLen);
            DataCopy(local_src0[base + halfLen], gm_a0[rowLo], halfLen);
            DataCopy(local_src1[base], gm_a1[rowHi], halfLen);
            DataCopy(local_src1[base + halfLen], gm_a1[rowLo], halfLen);
            KYBER_PIPE_ALL();
        }

        in_src0.EnQue(local_src0);
        in_src1.EnQue(local_src1);
        KYBER_PIPE_ALL();
    }

    /**
     * RouteA 移位累加 + 两次 Barrett；每 poly 独立。
     * 背景：槽 2/3 为零，等价单 poly 四 limb 合并。
     */
    __aicore__ inline void Compute()
    {
        LocalTensor<int32_t> local_src0 = in_src0.DeQue<int32_t>();
        LocalTensor<int32_t> local_src1 = in_src1.DeQue<int32_t>();
        LocalTensor<int32_t> local_dst = out_dst.AllocTensor<int32_t>();
        using AscendC::Add;
        using AscendC::ShiftLeft;

        for (uint16_t p = 0; p < kPolys; p++) {
            const uint32_t base = static_cast<uint32_t>(p) * polyStride;
            const uint32_t outOff = static_cast<uint32_t>(p) * halfLen;
            LocalTensor<int32_t> poly_dst = tmp_poly.AllocTensor<int32_t>();
            LocalTensor<int32_t> x[2][4];
            for (int j = 0; j < 4; j++) {
                x[0][j] = local_src0[base + static_cast<uint32_t>(j) * halfLen];
                x[1][j] = local_src1[base + static_cast<uint32_t>(j) * halfLen];
            }

            // 按 limb 权重左移后累加（RouteA）
            ShiftLeft(x[0][1], x[0][1], kKyberMergeShift1, halfLen);
            KYBER_PIPE_ALL();
            ShiftLeft(x[1][0], x[1][0], kKyberMergeShift1, halfLen);
            KYBER_PIPE_ALL();
            ShiftLeft(x[0][2], x[0][2], kKyberMergeShift2, halfLen);
            KYBER_PIPE_ALL();
            ShiftLeft(x[1][1], x[1][1], kKyberMergeShift2, halfLen);
            KYBER_PIPE_ALL();
            ShiftLeft(x[0][3], x[0][3], kKyberMergeShift3, halfLen);
            KYBER_PIPE_ALL();
            ShiftLeft(x[1][2], x[1][2], kKyberMergeShift3, halfLen);
            KYBER_PIPE_ALL();

            Add(poly_dst, x[0][0], x[0][1], halfLen);
            KYBER_PIPE_ALL();
            Add(poly_dst, poly_dst, x[1][0], halfLen);
            KYBER_PIPE_ALL();
            Add(poly_dst, poly_dst, x[0][2], halfLen);
            KYBER_PIPE_ALL();
            Add(poly_dst, poly_dst, x[1][1], halfLen);
            KYBER_PIPE_ALL();
            Add(poly_dst, poly_dst, x[0][3], halfLen);
            KYBER_PIPE_ALL();
            Add(poly_dst, poly_dst, x[1][2], halfLen);
            KYBER_PIPE_ALL();

            // 连调两次 Barrett（与 ntt_sim 约定）
            barrett_mul_vec_runtime(poly_dst, q, 12, 5039, local_src0, local_src1, static_cast<int32_t>(halfLen));
            KYBER_PIPE_ALL();
            barrett_mul_vec_runtime(poly_dst, q, 12, 5039, local_src0, local_src1, static_cast<int32_t>(halfLen));
            KYBER_PIPE_ALL();

            DataCopy(local_dst[outOff], poly_dst, halfLen);
            KYBER_PIPE_ALL();
            tmp_poly.FreeTensor(poly_dst);
        }

        out_dst.EnQue(local_dst);
        in_src0.FreeTensor(local_src0);
        in_src1.FreeTensor(local_src1);
        KYBER_PIPE_ALL();
    }

    /** 写回 dst 各 poly 的本 AIV 半段。 */
    __aicore__ inline void CopyOut()
    {
        LocalTensor<int32_t> local_dst = out_dst.DeQue<int32_t>();
        for (uint16_t p = 0; p < kPolys; p++) {
            const uint32_t dstOff = static_cast<uint32_t>(p) * coeffN + static_cast<uint32_t>(subCoreIdx) * halfLen;
            const uint32_t locOff = static_cast<uint32_t>(p) * halfLen;
            DataCopy(gm_dst[dstOff], local_dst[locOff], halfLen);
            KYBER_PIPE_ALL();
        }
        out_dst.FreeTensor(local_dst);
        KYBER_PIPE_ALL();
    }

private:
    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint16_t kPolys;
    const uint32_t halfLen;
    const int32_t q;
    const uint32_t polyStride;
    const uint32_t outTileLength;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> in_src0, in_src1;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst, tmp_poly;
    AscendC::GlobalTensor<int32_t> gm_a0, gm_a1;
    AscendC::GlobalTensor<int32_t> gm_dst;
};

#endif
