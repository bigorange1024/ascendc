#ifndef __AIV_FUNC_HPP__
#define __AIV_FUNC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "stage3_config.hpp"
#include "ntt_vec.hpp"

using AscendC::DataCopy;

/**
 * Stage1：Tag5T 自然序 batch — S0 紧凑 [HI_k | LO_k]；按系数半维 subCoreIdx 切分。
 * 历史对照探针：MLKEM 新实现须用 poly-batch 之 AivSplitPolyBatch（fix-f203-tag5t-ntt256-limb6-poly8-polybatch-s123）。
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
            const uint32_t hiOff = static_cast<uint32_t>(p) * coeffN + static_cast<uint32_t>(subCoreIdx) * halfLen;
            const uint32_t loOff =
                static_cast<uint32_t>(kPolys + p) * coeffN + static_cast<uint32_t>(subCoreIdx) * halfLen;
            const uint32_t locOff = static_cast<uint32_t>(p) * halfLen;
            /* split_vec: x0=lo, x1=hi；Tag5T 行 p=hi、k+p=lo */
            DataCopy(gm_s0[hiOff], local_dst1[locOff], halfLen);
            DataCopy(gm_s0[loOff], local_dst0[locOff], halfLen);
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
 * Stage3 RouteA（limb 面对半）：subCoreIdx==0 只读 C_lo、==1 只读 C_hi，各写 dst 半列。
 * 禁止用于 MLKEM 新代码（单 poly hi/lo 分属两 AIV）。生产路径：AivTag5tRouteAModPolyBatch。
 * 取模变体见 stage3_config.hpp F203_STAGE3_MOD、STAGE3_VARIANTS.md。
 */
class AivTag5tRouteAMod {
public:
    __aicore__ inline AivTag5tRouteAMod(int32_t subCoreIdx, uint32_t coeffN, uint16_t kPolys)
        : subCoreIdx(subCoreIdx), coeffN(coeffN), kPolys(kPolys), halfLen(coeffN / 2),
          polyStride(4 * (coeffN / 2)),
          outTileLength(static_cast<uint32_t>(kPolys) * (coeffN / 2))
    {
    }

    __aicore__ inline void Init(GM_ADDR dst, GM_ADDR cLo, GM_ADDR cHi)
    {
        gm_dst.SetGlobalBuffer((__gm__ int32_t *)dst);
        gm_mat.SetGlobalBuffer((__gm__ int32_t *)(subCoreIdx == 0 ? cLo : cHi));
        const uint32_t inBytes = static_cast<uint32_t>(kPolys) * polyStride * sizeof(int32_t);
        pipe.InitBuffer(in_limbs, 1, inBytes);
        pipe.InitBuffer(out_dst, 1, outTileLength * sizeof(int32_t));
        pipe.InitBuffer(row_buf, 1, coeffN * sizeof(int32_t));
        pipe.InitBuffer(gather_idx, 1, halfLen * sizeof(int32_t));
        pipe.InitBuffer(gather_idx2, 1, halfLen * sizeof(int32_t));
        pipe.InitBuffer(scratch_t1, 1, halfLen * sizeof(int32_t));
#if F203_STAGE3_MOD == 0
        pipe.InitBuffer(scratch_t2, 1, halfLen * sizeof(int32_t));
#elif F203_STAGE3_MOD == 2
        pipe.InitBuffer(calc_f, 3 * halfLen * sizeof(float));
#endif
        pipe.InitBuffer(tmp_poly, 1, halfLen * sizeof(int32_t));
    }

    __aicore__ inline void Process()
    {
        CopyIn();
        Compute();
        CopyOut();
    }

    __aicore__ inline void copy_row_even_odd(LocalTensor<int32_t> &even, LocalTensor<int32_t> &odd, uint32_t rowOff)
    {
        LocalTensor<int32_t> row = row_buf.AllocTensor<int32_t>();
        LocalTensor<int32_t> idx = gather_idx.AllocTensor<int32_t>();
        LocalTensor<int32_t> idx2 = gather_idx2.AllocTensor<int32_t>();
        DataCopy(row, gm_mat[rowOff], coeffN);
        KYBER_PIPE_ALL();
        deinterleave_even_odd_vec(even, odd, row, idx, idx2, static_cast<int32_t>(halfLen));
        KYBER_PIPE_ALL();
        row_buf.FreeTensor(row);
        gather_idx.FreeTensor(idx);
        gather_idx2.FreeTensor(idx2);
    }

    __aicore__ inline void CopyIn()
    {
        LocalTensor<int32_t> local_in = in_limbs.AllocTensor<int32_t>();
        for (uint16_t p = 0; p < kPolys; p++) {
            const uint32_t hiRow = static_cast<uint32_t>(p) * coeffN;
            const uint32_t loRow = static_cast<uint32_t>(kPolys + p) * coeffN;
            const uint32_t base = static_cast<uint32_t>(p) * polyStride;
            LocalTensor<int32_t> hh = local_in[base];
            LocalTensor<int32_t> lh = local_in[base + halfLen];
            LocalTensor<int32_t> hl = local_in[base + 2 * halfLen];
            LocalTensor<int32_t> ll = local_in[base + 3 * halfLen];
            copy_row_even_odd(hh, lh, hiRow);
            copy_row_even_odd(hl, ll, loRow);
        }
        in_limbs.EnQue(local_in);
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void Compute()
    {
        LocalTensor<int32_t> local_in = in_limbs.DeQue<int32_t>();
        LocalTensor<int32_t> local_dst = out_dst.AllocTensor<int32_t>();
        LocalTensor<int32_t> t1 = scratch_t1.AllocTensor<int32_t>();
#if F203_STAGE3_MOD == 0
        LocalTensor<int32_t> t2 = scratch_t2.AllocTensor<int32_t>();
#elif F203_STAGE3_MOD == 2
        const uint32_t fStride = halfLen * static_cast<uint32_t>(sizeof(float));
        LocalTensor<float> fRaw = calc_f.GetWithOffset<float>(halfLen, 0U);
        LocalTensor<float> fTmp = calc_f.GetWithOffset<float>(halfLen, fStride);
        LocalTensor<float> fQuot = calc_f.GetWithOffset<float>(halfLen, 2U * fStride);
#else
        LocalTensor<int32_t> t2 = t1;
#endif

        for (uint16_t p = 0; p < kPolys; p++) {
            const uint32_t base = static_cast<uint32_t>(p) * polyStride;
            const uint32_t outOff = static_cast<uint32_t>(p) * halfLen;
            LocalTensor<int32_t> hh = local_in[base];
            LocalTensor<int32_t> lh = local_in[base + halfLen];
            LocalTensor<int32_t> hl = local_in[base + 2 * halfLen];
            LocalTensor<int32_t> ll = local_in[base + 3 * halfLen];
            LocalTensor<int32_t> poly_dst = tmp_poly.AllocTensor<int32_t>();
#if F203_STAGE3_MOD == 2
            combine_limb6_routea_mod_vec(poly_dst, hh, lh, hl, ll, t1, fRaw, fTmp, fQuot, 3329,
                                         static_cast<int32_t>(halfLen));
#else
            combine_limb6_routea_mod_vec(poly_dst, hh, lh, hl, ll, t1, t2, 3329,
                                         static_cast<int32_t>(halfLen));
#endif
            KYBER_PIPE_ALL();
            DataCopy(local_dst[outOff], poly_dst, halfLen);
            KYBER_PIPE_ALL();
            tmp_poly.FreeTensor(poly_dst);
        }

        out_dst.EnQue(local_dst);
        in_limbs.FreeTensor(local_in);
        scratch_t1.FreeTensor(t1);
#if F203_STAGE3_MOD == 0
        scratch_t2.FreeTensor(t2);
#endif
        KYBER_PIPE_ALL();
    }

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
    const uint32_t polyStride;
    const uint32_t outTileLength;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> in_limbs;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> row_buf;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> gather_idx, gather_idx2;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t1;
#if F203_STAGE3_MOD == 0
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t2;
#elif F203_STAGE3_MOD == 2
    AscendC::TBuf<AscendC::TPosition::VECCALC> calc_f;
#endif
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst, tmp_poly;
    AscendC::GlobalTensor<int32_t> gm_mat, gm_dst;
};

#endif
