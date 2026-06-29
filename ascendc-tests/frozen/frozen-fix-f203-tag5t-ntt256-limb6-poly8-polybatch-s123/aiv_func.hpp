#ifndef __AIV_FUNC_HPP__
#define __AIV_FUNC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "ntt_vec.hpp"
#include "stage3_config.hpp"
#include "tiling.h"

using AscendC::DataCopy;

/**
 * Stage1 poly-batch（本仓 MLKEM NTT 权威）：AIV0 → poly 0..3 → S0 行 0..7；AIV1 → poly 4..7 → 行 8..15。
 * 每 poly 整行 256 系数 limb6 编码（非系数维对半）。定稿见 docs/notes/MLKEM-NTT-实现总结.md。
 */
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
        for (uint16_t lp = 0; lp < kPolysLocal; lp++) {
            const uint16_t p = static_cast<uint16_t>(polyStart + lp);
            const uint32_t srcOff = static_cast<uint32_t>(p) * coeffN;
            const uint32_t locOff = static_cast<uint32_t>(lp) * coeffN;
            DataCopy(local_src[locOff], gm_src[srcOff], coeffN);
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
        const uint32_t batch = static_cast<uint32_t>(subCoreIdx);
        const uint32_t rowBase = batch * static_cast<uint32_t>(tiling::s0RowsPerAiv);
        for (uint16_t lp = 0; lp < kPolysLocal; lp++) {
            const uint32_t hiRow = rowBase + static_cast<uint32_t>(lp);
            const uint32_t loRow = rowBase + static_cast<uint32_t>(tiling::kPolysPerAiv) + static_cast<uint32_t>(lp);
            const uint32_t locOff = static_cast<uint32_t>(lp) * coeffN;
            DataCopy(gm_s0[hiRow * coeffN], local_dst1[locOff], coeffN);
            DataCopy(gm_s0[loRow * coeffN], local_dst0[locOff], coeffN);
            KYBER_PIPE_ALL();
        }
        out_dst0.FreeTensor(local_dst0);
        out_dst1.FreeTensor(local_dst1);
        KYBER_PIPE_ALL();
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
 * Stage3 poly-batch（本仓 MLKEM NTT 权威）：每 AIV 读本批 C_lo 8 行 + C_hi 8 行，UB 内握完整 poly hi+lo。
 * 禁止单 poly 高低位分属不同 AIV。无 AIV↔AIV 同步。块内行序 [hi0..hi3,lo0..lo3]；C_lo→[0:128)、C_hi→[128:256)。
 */
class AivTag5tRouteAModPolyBatch {
public:
    __aicore__ inline AivTag5tRouteAModPolyBatch(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx(subCoreIdx), coeffN(coeffN),
          kPolysLocal(static_cast<uint16_t>(tiling::kPolysPerAiv)),
          polyStart(static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kPolysPerAiv)),
          halfLen(coeffN / 2), polyStride(4 * (coeffN / 2)),
          outTileLength(static_cast<uint32_t>(tiling::kPolysPerAiv) * coeffN)
    {
    }

    __aicore__ inline void Init(GM_ADDR dst, GM_ADDR mat_c)
    {
        gm_dst.SetGlobalBuffer((__gm__ int32_t *)dst);
        const uint32_t rowBytes = coeffN * static_cast<uint32_t>(sizeof(int32_t));
        const uint32_t batch = static_cast<uint32_t>(subCoreIdx);
        const uint32_t loPlaneRows = static_cast<uint32_t>(tiling::mRows);
        gm_cLo.SetGlobalBuffer((__gm__ int32_t *)(mat_c + batch * tiling::s0RowsPerAiv * rowBytes));
        gm_cHi.SetGlobalBuffer(
            (__gm__ int32_t *)(mat_c + (loPlaneRows + batch * tiling::s0RowsPerAiv) * rowBytes));
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
        pipe.InitBuffer(limb_scratch, 1, polyStride * sizeof(int32_t));
    }

    __aicore__ inline void Process()
    {
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

        for (uint16_t lp = 0; lp < kPolysLocal; lp++) {
            const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN;
#if F203_STAGE3_MOD == 2
            merge_stack_half(gm_cLo, lp, t1, fRaw, fTmp, fQuot, local_dst[outBase]);
            merge_stack_half(gm_cHi, lp, t1, fRaw, fTmp, fQuot, local_dst[outBase + halfLen]);
#else
            merge_stack_half(gm_cLo, lp, t1, t2, local_dst[outBase]);
            merge_stack_half(gm_cHi, lp, t1, t2, local_dst[outBase + halfLen]);
#endif
        }

        out_dst.EnQue(local_dst);
        scratch_t1.FreeTensor(t1);
#if F203_STAGE3_MOD == 0
        scratch_t2.FreeTensor(t2);
#endif
        KYBER_PIPE_ALL();
        CopyOut();
    }

private:
    __aicore__ inline void copy_row_even_odd(AscendC::GlobalTensor<int32_t> &gm_mat, uint32_t rowOff,
                                             LocalTensor<int32_t> &even, LocalTensor<int32_t> &odd)
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

#if F203_STAGE3_MOD == 2
    __aicore__ inline void merge_stack_half(AscendC::GlobalTensor<int32_t> &gm_mat, uint16_t lp,
                                            LocalTensor<int32_t> &t1, LocalTensor<float> &fRaw,
                                            LocalTensor<float> &fTmp, LocalTensor<float> &fQuot,
                                            LocalTensor<int32_t> outHalf)
#else
    __aicore__ inline void merge_stack_half(AscendC::GlobalTensor<int32_t> &gm_mat, uint16_t lp,
                                            LocalTensor<int32_t> &t1, LocalTensor<int32_t> &t2,
                                            LocalTensor<int32_t> outHalf)
#endif
    {
        LocalTensor<int32_t> limbs = limb_scratch.AllocTensor<int32_t>();
        const uint32_t hiOff = static_cast<uint32_t>(lp) * coeffN;
        const uint32_t loOff = (static_cast<uint32_t>(kPolysLocal) + static_cast<uint32_t>(lp)) * coeffN;
        LocalTensor<int32_t> hh = limbs[0];
        LocalTensor<int32_t> lh = limbs[halfLen];
        LocalTensor<int32_t> hl = limbs[2 * halfLen];
        LocalTensor<int32_t> ll = limbs[3 * halfLen];
        copy_row_even_odd(gm_mat, hiOff, hh, lh);
        copy_row_even_odd(gm_mat, loOff, hl, ll);
        LocalTensor<int32_t> poly_dst = tmp_poly.AllocTensor<int32_t>();
#if F203_STAGE3_MOD == 2
        combine_limb6_routea_mod_vec(poly_dst, hh, lh, hl, ll, t1, fRaw, fTmp, fQuot, 3329,
                                     static_cast<int32_t>(halfLen));
#else
        combine_limb6_routea_mod_vec(poly_dst, hh, lh, hl, ll, t1, t2, 3329, static_cast<int32_t>(halfLen));
#endif
        KYBER_PIPE_ALL();
        DataCopy(outHalf, poly_dst, halfLen);
        KYBER_PIPE_ALL();
        tmp_poly.FreeTensor(poly_dst);
        limb_scratch.FreeTensor(limbs);
    }

    __aicore__ inline void CopyOut()
    {
        LocalTensor<int32_t> local_dst = out_dst.DeQue<int32_t>();
        for (uint16_t lp = 0; lp < kPolysLocal; lp++) {
            const uint16_t p = static_cast<uint16_t>(polyStart + lp);
            const uint32_t dstOff = static_cast<uint32_t>(p) * coeffN;
            const uint32_t locOff = static_cast<uint32_t>(lp) * coeffN;
            DataCopy(gm_dst[dstOff], local_dst[locOff], coeffN);
            KYBER_PIPE_ALL();
        }
        out_dst.FreeTensor(local_dst);
        KYBER_PIPE_ALL();
    }

    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint16_t kPolysLocal;
    const uint16_t polyStart;
    const uint32_t halfLen;
    const uint32_t polyStride;
    const uint32_t outTileLength;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> row_buf;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> gather_idx, gather_idx2;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t1;
#if F203_STAGE3_MOD == 0
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t2;
#elif F203_STAGE3_MOD == 2
    AscendC::TBuf<AscendC::TPosition::VECCALC> calc_f;
#endif
    AscendC::TQue<AscendC::TPosition::VECIN, 1> limb_scratch;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst, tmp_poly;
    AscendC::GlobalTensor<int32_t> gm_cLo, gm_cHi, gm_dst;
};

#endif
