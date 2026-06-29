#ifndef __AIV_FUNC_HPP__
#define __AIV_FUNC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "ntt_vec.hpp"
#include "mod_config.hpp"
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

namespace planar {

__aicore__ inline uint32_t s0_hi_row(uint32_t sub, uint32_t lp)
{
    return sub * static_cast<uint32_t>(tiling::s0RowsPerAiv) + lp;
}

__aicore__ inline uint32_t s0_lo_row(uint32_t sub, uint32_t lp)
{
    return sub * static_cast<uint32_t>(tiling::s0RowsPerAiv) + static_cast<uint32_t>(tiling::kPolysPerAiv) + lp;
}

__aicore__ inline uint32_t mat_row(uint32_t polyIdx, uint32_t limb, uint32_t half)
{
    return half * static_cast<uint32_t>(tiling::kPolys) * tiling::kLimbsPerPoly + polyIdx * tiling::kLimbsPerPoly +
           limb;
}

} // namespace planar

/**
 * Stage2 收尾：Cube 偶/奇列分乘结果重排为平面 mat_c [64,128]（无 Gather）。
 */
class AivPackMatCPlanar {
public:
    __aicore__ inline AivPackMatCPlanar(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx(subCoreIdx), coeffN(coeffN), halfLen(coeffN / 2),
          limbTileLength(4 * (coeffN / 2)),
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
        pipe.InitBuffer(que_limb_tile, 1, limbTileLength * sizeof(int32_t));
    }

    __aicore__ inline void pack_poly_half(uint32_t polyIdx, uint32_t hiR, uint32_t loR, uint32_t half,
                                          AscendC::GlobalTensor<int32_t> &gmEven,
                                          AscendC::GlobalTensor<int32_t> &gmOdd)
    {
        LocalTensor<int32_t> tile = que_limb_tile.AllocTensor<int32_t>();
        DataCopy(tile[0], gmEven[hiR * halfLen], halfLen);
        DataCopy(tile[halfLen], gmOdd[hiR * halfLen], halfLen);
        DataCopy(tile[2 * halfLen], gmEven[loR * halfLen], halfLen);
        DataCopy(tile[3 * halfLen], gmOdd[loR * halfLen], halfLen);
        KYBER_PIPE_ALL();
        const uint32_t dstBase = planar::mat_row(polyIdx, 0U, half) * halfLen;
        DataCopy(gm_out[dstBase], tile, limbTileLength);
        KYBER_PIPE_ALL();
        que_limb_tile.FreeTensor(tile);
    }

    __aicore__ inline void Process()
    {
        const uint32_t sub = static_cast<uint32_t>(subCoreIdx);
        for (uint16_t lp = 0; lp < kPolysLocal; ++lp) {
            const uint32_t polyIdx = static_cast<uint32_t>(polyStart) + static_cast<uint32_t>(lp);
            const uint32_t hiR = planar::s0_hi_row(sub, static_cast<uint32_t>(lp));
            const uint32_t loR = planar::s0_lo_row(sub, static_cast<uint32_t>(lp));
            pack_poly_half(polyIdx, hiR, loR, 0U, gm_lo_even, gm_lo_odd);
            pack_poly_half(polyIdx, hiR, loR, 1U, gm_hi_even, gm_hi_odd);
        }
    }

private:
    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint32_t halfLen;
    const uint32_t limbTileLength;
    const uint16_t polyStart;
    const uint16_t kPolysLocal;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_limb_tile;
    AscendC::GlobalTensor<int32_t> gm_out;
    AscendC::GlobalTensor<int32_t> gm_lo_even, gm_lo_odd, gm_hi_even, gm_hi_odd;
};

/**
 * Stage3 poly-batch：平面 mat_c [64,128]，每 poly 每 half 一次 bulk 读四 limb 行，无 Gather。
 */
class AivTag5tRouteAModPolyBatch {
public:
    __aicore__ inline AivTag5tRouteAModPolyBatch(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx(subCoreIdx), coeffN(coeffN),
          kPolysLocal(static_cast<uint16_t>(tiling::kPolysPerAiv)),
          polyStart(static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kPolysPerAiv)),
          halfLen(coeffN / 2), limbTileLength(4 * (coeffN / 2)),
          batchHalfPlaneLength(static_cast<uint32_t>(tiling::kPolysPerAiv) * 4 * (coeffN / 2)),
          outTileLength(static_cast<uint32_t>(tiling::kPolysPerAiv) * coeffN)
    {
    }

    __aicore__ inline void Init(GM_ADDR dst, GM_ADDR matPlanar)
    {
        gm_dst.SetGlobalBuffer((__gm__ int32_t *)dst);
        gm_planar.SetGlobalBuffer((__gm__ int32_t *)matPlanar);
        pipe.InitBuffer(out_dst, 1, outTileLength * sizeof(int32_t));
        pipe.InitBuffer(scratch_t1, 1, halfLen * sizeof(int32_t));
#if F203_MOD_VARIANT == 0
        pipe.InitBuffer(scratch_t2, 1, halfLen * sizeof(int32_t));
#elif F203_MOD_VARIANT == 2
        pipe.InitBuffer(calc_f, 3 * halfLen * sizeof(float));
#endif
        pipe.InitBuffer(half_plane, 1, batchHalfPlaneLength * sizeof(int32_t));
        pipe.InitBuffer(limb_scratch, 1, limbTileLength * sizeof(int32_t));
        pipe.InitBuffer(tmp_half, 1, halfLen * sizeof(int32_t));
    }

    __aicore__ inline void Process()
    {
        LocalTensor<int32_t> local_dst = out_dst.AllocTensor<int32_t>();
        LocalTensor<int32_t> t1 = scratch_t1.AllocTensor<int32_t>();
#if F203_MOD_VARIANT == 0
        LocalTensor<int32_t> t2 = scratch_t2.AllocTensor<int32_t>();
#elif F203_MOD_VARIANT == 2
        const uint32_t fStride = halfLen * static_cast<uint32_t>(sizeof(float));
        LocalTensor<float> fRaw = calc_f.GetWithOffset<float>(halfLen, 0U);
        LocalTensor<float> fTmp = calc_f.GetWithOffset<float>(halfLen, fStride);
        LocalTensor<float> fQuot = calc_f.GetWithOffset<float>(halfLen, 2U * fStride);
#else
        LocalTensor<int32_t> t2 = t1;
#endif

        LocalTensor<int32_t> plane = half_plane.AllocTensor<int32_t>();
        const uint32_t loPlaneBase = planar::mat_row(static_cast<uint32_t>(polyStart), 0U, 0U) * halfLen;
        const uint32_t hiPlaneBase = planar::mat_row(static_cast<uint32_t>(polyStart), 0U, 1U) * halfLen;
        DataCopy(plane, gm_planar[loPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();

        LocalTensor<int32_t> half_out = tmp_half.AllocTensor<int32_t>();
        for (uint16_t lp = 0; lp < kPolysLocal; lp++) {
            const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN;
            const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength;
            LocalTensor<int32_t> limbs = limb_scratch.AllocTensor<int32_t>();
            DataCopy(limbs, plane[limbOff], limbTileLength);
            KYBER_PIPE_ALL();
            LocalTensor<int32_t> hh = limbs[0];
            LocalTensor<int32_t> lh = limbs[halfLen];
            LocalTensor<int32_t> hl = limbs[2 * halfLen];
            LocalTensor<int32_t> ll = limbs[3 * halfLen];
#if F203_MOD_VARIANT == 2
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, t1, fRaw, fTmp, fQuot, 3329,
                                         static_cast<int32_t>(halfLen));
#else
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, t2, 3329, static_cast<int32_t>(halfLen));
#endif
            KYBER_PIPE_ALL();
            DataCopy(local_dst[outBase], half_out, halfLen);
            KYBER_PIPE_ALL();
            limb_scratch.FreeTensor(limbs);
        }

        DataCopy(plane, gm_planar[hiPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();
        for (uint16_t lp = 0; lp < kPolysLocal; lp++) {
            const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN;
            const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength;
            LocalTensor<int32_t> limbs = limb_scratch.AllocTensor<int32_t>();
            DataCopy(limbs, plane[limbOff], limbTileLength);
            KYBER_PIPE_ALL();
            LocalTensor<int32_t> hh = limbs[0];
            LocalTensor<int32_t> lh = limbs[halfLen];
            LocalTensor<int32_t> hl = limbs[2 * halfLen];
            LocalTensor<int32_t> ll = limbs[3 * halfLen];
#if F203_MOD_VARIANT == 2
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, t1, fRaw, fTmp, fQuot, 3329,
                                         static_cast<int32_t>(halfLen));
#else
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, t2, 3329, static_cast<int32_t>(halfLen));
#endif
            KYBER_PIPE_ALL();
            DataCopy(local_dst[outBase + halfLen], half_out, halfLen);
            KYBER_PIPE_ALL();
            limb_scratch.FreeTensor(limbs);
        }
        tmp_half.FreeTensor(half_out);
        half_plane.FreeTensor(plane);

        out_dst.EnQue(local_dst);
        scratch_t1.FreeTensor(t1);
#if F203_MOD_VARIANT == 0
        scratch_t2.FreeTensor(t2);
#endif
        KYBER_PIPE_ALL();
        CopyOut();
    }

private:
    __aicore__ inline void CopyOut()
    {
        LocalTensor<int32_t> local_dst = out_dst.DeQue<int32_t>();
        const uint32_t dstBase = static_cast<uint32_t>(polyStart) * coeffN;
        DataCopy(gm_dst[dstBase], local_dst, outTileLength);
        KYBER_PIPE_ALL();
        out_dst.FreeTensor(local_dst);
    }

    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint16_t kPolysLocal;
    const uint16_t polyStart;
    const uint32_t halfLen;
    const uint32_t limbTileLength;
    const uint32_t batchHalfPlaneLength;
    const uint32_t outTileLength;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t1;
#if F203_MOD_VARIANT == 0
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t2;
#elif F203_MOD_VARIANT == 2
    AscendC::TBuf<AscendC::TPosition::VECCALC> calc_f;
#endif
    AscendC::TQue<AscendC::TPosition::VECIN, 1> half_plane;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> limb_scratch;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> tmp_half;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst;
    AscendC::GlobalTensor<int32_t> gm_planar, gm_dst;
};

#endif
