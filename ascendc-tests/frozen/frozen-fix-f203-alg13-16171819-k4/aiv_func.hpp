#ifndef __AIV_FUNC_HPP__
#define __AIV_FUNC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"
#include "mod_config.hpp"
#include "hat_debug_config.hpp"
#include "hat_vec.hpp"
#include "ntt_vec.hpp"

using AscendC::DataCopy;

/**
 * Stage1（待迁 poly-batch）：当前 limbsplit-s123 系数半维切分。目标 AivSplitPolyBatch；
 * 见 docs/notes/MLKEM-NTT-实现总结.md、qa/TODO T11。
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
 * Stage3 RouteA：整行 DataCopy + Gather 解交织 + 合并/mod（见 mod_config.hpp F203_MOD_VARIANT）。
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
#if F203_MOD_VARIANT == 1
        pipe.InitBuffer(scratch_t2, 1, halfLen * sizeof(int32_t));
#elif F203_MOD_VARIANT == 2
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
#if F203_MOD_VARIANT == 1
        LocalTensor<int32_t> t2 = scratch_t2.AllocTensor<int32_t>();
#elif F203_MOD_VARIANT == 2
        LocalTensor<int32_t> t2 = t1;
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
#if F203_MOD_VARIANT == 1
            combine_limb6_routea_mod_vec(poly_dst, hh, lh, hl, ll, t1, t2, 3329,
                                         static_cast<int32_t>(halfLen));
#elif F203_MOD_VARIANT == 2
            combine_limb6_routea_mod_vec(poly_dst, hh, lh, hl, ll, t1, t2, fRaw, fTmp, fQuot, 3329,
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
#if F203_MOD_VARIANT == 1
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
#if F203_MOD_VARIANT == 1
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t2;
#elif F203_MOD_VARIANT == 2
    AscendC::TBuf<AscendC::TPosition::VECCALC> calc_f;
#endif
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst, tmp_poly;
    AscendC::GlobalTensor<int32_t> gm_mat, gm_dst;
};

/** Alg.13 行 18：$\hat{t}=\hat{A}\circ\hat{s}+\hat{e}$（k=4，AIV basemul + lazy ∑ + mod）。 */
class AivHatLine18 {
public:
    __aicore__ inline AivHatLine18(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx(subCoreIdx), coeffN(coeffN), halfLen(coeffN / 2), pairCount(halfLen / 2),
          gammaOff(static_cast<int32_t>(subCoreIdx) * static_cast<int32_t>(halfLen / 2)),
          subOff(static_cast<uint32_t>(subCoreIdx) * halfLen)
    {
    }

    __aicore__ inline void Init(GM_ADDR t_hat, GM_ADDR shat_ehat, GM_ADDR a_hat)
    {
        gm_t.SetGlobalBuffer((__gm__ int32_t *)t_hat);
        gm_se.SetGlobalBuffer((__gm__ int32_t *)shat_ehat);
        gm_a.SetGlobalBuffer((__gm__ int32_t *)a_hat);
        pipe.InitBuffer(que_acc, 1, halfLen * sizeof(int32_t));
        pipe.InitBuffer(que_row, 1, halfLen * sizeof(int32_t));
        const uint32_t pc = pairCount;
        const uint32_t hl = halfLen;
        pipe.InitBuffer(scratch, (10U * pc + 4U * hl) * sizeof(int32_t));
#if F203_MOD_VARIANT == 2
        pipe.InitBuffer(calc_f, 3U * hl * sizeof(float));
#endif
    }

    __aicore__ inline void Process()
    {
        const uint32_t pc = pairCount;
        const uint32_t pcBytes = pc * sizeof(int32_t);
        const uint32_t hlBytes = halfLen * sizeof(int32_t);
        LocalTensor<int32_t> acc = que_acc.AllocTensor<int32_t>();
        LocalTensor<int32_t> row = que_row.AllocTensor<int32_t>();
        LocalTensor<int32_t> a0 = scratch.GetWithOffset<int32_t>(pc, 0U);
        LocalTensor<int32_t> a1 = scratch.GetWithOffset<int32_t>(pc, pcBytes);
        LocalTensor<int32_t> b0 = scratch.GetWithOffset<int32_t>(pc, 2U * pcBytes);
        LocalTensor<int32_t> b1 = scratch.GetWithOffset<int32_t>(pc, 3U * pcBytes);
        LocalTensor<int32_t> c0 = scratch.GetWithOffset<int32_t>(pc, 4U * pcBytes);
        LocalTensor<int32_t> c1 = scratch.GetWithOffset<int32_t>(pc, 5U * pcBytes);
        LocalTensor<int32_t> t1 = scratch.GetWithOffset<int32_t>(pc, 6U * pcBytes);
        LocalTensor<int32_t> t2 = scratch.GetWithOffset<int32_t>(pc, 7U * pcBytes);
        LocalTensor<int32_t> idx = scratch.GetWithOffset<int32_t>(pc, 8U * pcBytes);
        LocalTensor<int32_t> idx2 = scratch.GetWithOffset<int32_t>(pc, 9U * pcBytes);
        const uint32_t baseFG = 10U * pcBytes;
        LocalTensor<int32_t> f = scratch.GetWithOffset<int32_t>(halfLen, baseFG);
        LocalTensor<int32_t> g = scratch.GetWithOffset<int32_t>(halfLen, baseFG + hlBytes);
        LocalTensor<int32_t> t1m = scratch.GetWithOffset<int32_t>(halfLen, baseFG + 2U * hlBytes);
        LocalTensor<int32_t> t2m = scratch.GetWithOffset<int32_t>(halfLen, baseFG + 3U * hlBytes);
#if F203_MOD_VARIANT == 2
        const uint32_t fStride = halfLen * static_cast<uint32_t>(sizeof(float));
        LocalTensor<float> fRaw = calc_f.GetWithOffset<float>(halfLen, 0U);
        LocalTensor<float> fTmp = calc_f.GetWithOffset<float>(halfLen, fStride);
        LocalTensor<float> fQuot = calc_f.GetWithOffset<float>(halfLen, 2U * fStride);
#endif

        for (uint16_t p = 0; p < tiling::kHatK; ++p) {
            AscendC::Duplicate(acc, static_cast<int32_t>(0), static_cast<int32_t>(halfLen));
            KYBER_PIPE_ALL();
            for (uint16_t j = 0; j < tiling::kHatK; ++j) {
                const uint32_t aRow = (static_cast<uint32_t>(p) * tiling::kHatK + j) * coeffN + subOff;
                const uint32_t sRow = static_cast<uint32_t>(j) * coeffN + subOff;
                DataCopy(f, gm_a[aRow], halfLen);
                DataCopy(g, gm_se[sRow], halfLen);
                KYBER_PIPE_ALL();
                /* basemul：仅 scalar（multiply_ntts_half_vec 已注释，见 hat_vec.hpp） */
                multiply_ntts_half_scalar(row, f, g, static_cast<int32_t>(pairCount), gammaOff);
                AscendC::Add(acc, acc, row, static_cast<int32_t>(halfLen));
                KYBER_PIPE_ALL();
#if HAT_DEBUG_PRINT
                if (p == 0 && j == 0) {
                    AscendC::printf("[HAT] core=%d p0j0_prod_off=%u:", subCoreIdx, subOff);
                    for (int32_t di = 0; di < 8; ++di) {
                        AscendC::printf(" %d", row.GetValue(di));
                    }
                    AscendC::printf("\n");
                }
#endif
            }
#if HAT_DEBUG_PRINT
            if (p == 0) {
                AscendC::printf("[HAT] core=%d p0_lazy_acc_off=%u:", subCoreIdx, subOff);
                for (int32_t di = 0; di < 8; ++di) {
                    AscendC::printf(" %d", acc.GetValue(di));
                }
                AscendC::printf("\n");
            }
#endif
#if !HAT_DEBUG_INNER_ONLY
            const uint32_t eRow = (tiling::kHatK + static_cast<uint32_t>(p)) * coeffN + subOff;
            DataCopy(row, gm_se[eRow], halfLen);
            KYBER_PIPE_ALL();
#if HAT_DEBUG_PRINT
            if (p == 1 && subCoreIdx == 1) {
                AscendC::printf("[HAT] p1c1 pre_e acc[5]=%d e[5]=%d\n", acc.GetValue(5), row.GetValue(5));
            }
#endif
            AscendC::Add(acc, acc, row, static_cast<int32_t>(halfLen));
            KYBER_PIPE_ALL();
#if HAT_DEBUG_PRINT
            if (p == 1 && subCoreIdx == 1) {
                AscendC::printf("[HAT] p1c1 post_e acc[5]=%d\n", acc.GetValue(5));
            }
#endif
#if F203_MOD_VARIANT == 2
            MOD_Q_CAST(acc, kHatQ, t1m, fRaw, fTmp, fQuot, static_cast<int32_t>(halfLen));
#else
            MOD_Q_I32(acc, kHatQ, t1m, t2m, static_cast<int32_t>(halfLen));
#endif
            KYBER_PIPE_ALL();
#if HAT_DEBUG_PRINT
            if (p == 1 && subCoreIdx == 1) {
                AscendC::printf("[HAT] p1c1 post_mod acc[5]=%d\n", acc.GetValue(5));
            }
#endif
#endif
            const uint32_t outRow = static_cast<uint32_t>(p) * coeffN + subOff;
            DataCopy(gm_t[outRow], acc, halfLen);
            KYBER_PIPE_ALL();
        }

        que_acc.FreeTensor(acc);
        que_row.FreeTensor(row);
        KYBER_PIPE_ALL();
    }

private:
    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint32_t halfLen;
    const uint32_t pairCount;
    const int32_t gammaOff;
    const uint32_t subOff;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_acc, que_row;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratch;
#if F203_MOD_VARIANT == 2
    AscendC::TBuf<AscendC::TPosition::VECCALC> calc_f;
#endif
    AscendC::GlobalTensor<int32_t> gm_t, gm_se, gm_a;
};

#endif
