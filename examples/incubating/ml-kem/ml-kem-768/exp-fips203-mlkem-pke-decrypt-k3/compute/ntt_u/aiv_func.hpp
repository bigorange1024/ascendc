/**
 * @file aiv_func.hpp
 * @brief Stage1/2后/Stage3 AIV：poly-batch limb 编码、平面 mat_c 打包、RouteA+mod。
 *
 * 流水线位置（与 AIC CrossCore 配合）：
 *   AivK8Split → AIC MMAD → AivK8PackMatCPlanar → AivK8RouteAMod
 *
 * 几何：本探针 tiling 中 kInttBatch=4、kPlanarSlots=4；NTT(r) 段仅用前 kK=3 行语义，
 * workspace 仍按 INTT batch=4 分配。poly-batch：每 AIV 握完整 poly 的 hi+lo（非 limbsplit）。
 *
 * Golden：经 NTT/INTT kernel 写 y_hat / u / v；本文件无独立 I/O。
 */
#ifndef STAGE123_POLYVEC8_AIV_FUNC_HPP
#define STAGE123_POLYVEC8_AIV_FUNC_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "ntt_vec.hpp"
#include "stage1_config.hpp"
#include "stage3_config.hpp"
#include "f203_decrypt_ntt_u_tiling.h"

using AscendC::DataCopy;

namespace planar_k8 {

/**
 * 平面 mat_c 行号：slot×4 limb × half(lo/hi)。
 * @param slot poly 槽位 [0,kPlanarSlots)；@param limb [0,4)；@param half 0=lo 半平面 / 1=hi
 * @return 行索引，乘 halfLen 得 int32 元素偏移
 */
__aicore__ inline uint32_t mat_row(uint32_t slot, uint32_t limb, uint32_t half)
{
    return half * static_cast<uint32_t>(tiling::kPlanarSlots) * tiling::kLimbsPerPoly +
           slot * tiling::kLimbsPerPoly + limb;
}

} // namespace planar_k8

/** NTT(r) 的真实分片：AIV0 处理 poly 0/1，AIV1 只处理 poly 2；分配容量仍按 2 行。 */
__aicore__ inline uint32_t ntt_poly_base(int32_t subCoreIdx)
{
    return (subCoreIdx == 0) ? 0U : 2U;
}

/** @return 当前 AIV 在 NTT(r) 阶段的真实 poly 数，禁止把第 4 行当假 poly 读写。 */
__aicore__ inline uint16_t ntt_poly_count(int32_t subCoreIdx)
{
    return (subCoreIdx == 0) ? 2U : 1U;
}

/**
 * Stage1：int32 系数 → hi/lo 各 6-bit limb（int8）写入 S0。
 *
 * 输入：GM src [kK 或 kInttBatch, N] int32（NTT 用 y / INTT 用 û）；
 * 或 ProcessFromLocal：UB 已驻留的 halfrows（融合核内积后不经 GM）。
 * 输出：wsS0 紧凑 [HI 行 0..k-1 | LO 行 k..2k-1] int8，供 AIC MMAD。
 * 前置：subCoreIdx∈{0,1}；本核处理 polyBase 起 kPolysPerAiv 条 poly。
 */
class AivK8Split {
public:
    /**
     * @param subCoreIdx AIV 子核 0/1；@param coeffN 系数个数（通常 256）
     * polyBase_：AIV0→0，AIV1→2；AIV1 仅处理 1 条 poly。
     */
    __aicore__ inline AivK8Split(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN), polyBase_(ntt_poly_base(subCoreIdx)),
          polyCount_(ntt_poly_count(subCoreIdx))
    {
    }

    /**
     * 绑定 GM 并分配 VECIN/VECOUT（及可选 splitScratch）。
     * @param wsS0 Stage1 输出 S0；@param src 输入系数 GM（ProcessFromLocal 时仍需合法指针）
     */
    __aicore__ inline void Init(GM_ADDR wsS0, GM_ADDR src)
    {
        (void)src;
        gm_src_.SetGlobalBuffer((__gm__ int32_t *)src);
        gm_s0_.SetGlobalBuffer((__gm__ int8_t *)wsS0);
        const uint32_t inBytes = tiling::kPolysPerAiv * coeffN_ * sizeof(int32_t);
        const uint32_t outBytes = tiling::kPolysPerAiv * coeffN_ * sizeof(int8_t);
        pipe_.InitBuffer(inQ_, 1, inBytes);
        pipe_.InitBuffer(out0_, 1, outBytes);
        pipe_.InitBuffer(out1_, 1, outBytes);
#if F203_STAGE1_SPLIT >= 1
        const uint32_t maxBank = tiling::kPolysPerAiv * coeffN_;
        const uint32_t scratchBytes = maxBank * (3U * sizeof(int32_t) + sizeof(int16_t) + sizeof(half));
        pipe_.InitBuffer(splitScratch_, scratchBytes);
#endif
    }

    /** 从 GM src 读本核 poly bank → 编码写 S0 */
    __aicore__ inline void Process()
    {
        encodeBank(polyBase_, polyCount_);
    }

    /**
     * INTT S1：û 已在 UB（内积驻留），不经 uNtt GM 绕路。
     * @param local_src [kPolysPerAiv, coeffN] int32，与本核 halfrows 对齐。
     */
    __aicore__ inline void ProcessFromLocal(AscendC::LocalTensor<int32_t> &local_src)
    {
        encodeCore(polyBase_, static_cast<uint16_t>(tiling::kPolysPerAiv), local_src);
    }

private:
    /**
     * 核心编码：split_vec → lo/hi int8 → 按 poly 写 S0 的 hi 行与 lo 行。
     * lo 行偏移 = kK + polyBase + lp；INTT batch4 使用专用类，避免混用 NTT k=3 lo 基准。
     */
    __aicore__ inline void encodeCore(uint32_t polyBase, uint16_t kPolys, AscendC::LocalTensor<int32_t> &local_src)
    {
        LocalTensor<int8_t> local_dst0 = out0_.AllocTensor<int8_t>();
        LocalTensor<int8_t> local_dst1 = out1_.AllocTensor<int8_t>();
        Tensor_int8x4 res{local_dst0, local_dst1};
#if F203_STAGE1_SPLIT >= 1
        const uint32_t elemCount = static_cast<uint32_t>(kPolys) * coeffN_;
        split_vec(res, local_src, static_cast<int32_t>(elemCount), splitScratch_, 32);
#else
        split_vec(res, local_src, static_cast<int32_t>(static_cast<uint32_t>(kPolys) * coeffN_));
#endif
        out0_.EnQue(local_dst0);
        out1_.EnQue(local_dst1);

        local_dst0 = out0_.DeQue<int8_t>();
        local_dst1 = out1_.DeQue<int8_t>();
        // 写回 S0：hi 行在 [0,k)，lo 行在 [k,2k)；每 poly 一次 DataCopy + PIPE_ALL
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t hiRow = polyBase + static_cast<uint32_t>(lp);
            const uint32_t loRow = static_cast<uint32_t>(tiling::kK) + polyBase + static_cast<uint32_t>(lp);
            const uint32_t locOff = static_cast<uint32_t>(lp) * coeffN_;
            DataCopy(gm_s0_[hiRow * coeffN_], local_dst1[locOff], coeffN_);
            DataCopy(gm_s0_[loRow * coeffN_], local_dst0[locOff], coeffN_);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        out0_.FreeTensor(local_dst0);
        out1_.FreeTensor(local_dst1);
    }

    /** GM→UB 拷贝本核 bank，再 encodeCore */
    __aicore__ inline void encodeBank(uint32_t polyBase, uint16_t kPolys)
    {
        const uint32_t srcOff = polyBase * coeffN_;
        const uint32_t elemCount = static_cast<uint32_t>(kPolys) * coeffN_;

        LocalTensor<int32_t> local_src = inQ_.AllocTensor<int32_t>();
        DataCopy(local_src, gm_src_[srcOff], elemCount);
        AscendC::PipeBarrier<PIPE_ALL>();
        inQ_.EnQue(local_src);
        local_src = inQ_.DeQue<int32_t>();

        encodeCore(polyBase, kPolys, local_src);
        inQ_.FreeTensor(local_src);
    }

    int32_t subCoreIdx_;
    uint32_t coeffN_;
    uint32_t polyBase_;
    uint16_t polyCount_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out0_, out1_;
#if F203_STAGE1_SPLIT >= 1
    AscendC::TBuf<AscendC::TPosition::VECCALC> splitScratch_;
#endif
    AscendC::GlobalTensor<int32_t> gm_src_;
    AscendC::GlobalTensor<int8_t> gm_s0_;
};

/**
 * Stage2 后：Cube 四路临时（lo/hi × even/odd）→ 平面 mat_c。
 *
 * 输入：MAT_C_TMP_{LO,HI}_{EVEN,ODD}，每路 [mRows, halfN] int32；
 * 输出：matPlanar 按 mat_row(slot,limb,half) 排布，供 Stage3 RouteA。
 * 前置：AIC 四次 MMAD 已完成且 CrossCore 已通知 AIV。
 */
class AivK8PackMatCPlanar {
public:
    __aicore__ inline AivK8PackMatCPlanar(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN), halfLen_(coeffN / 2),
          polyBase_(ntt_poly_base(subCoreIdx)), polyCount_(ntt_poly_count(subCoreIdx)),
          limbTileLength_(4 * (coeffN / 2))
    {
    }

    /**
     * @param matPlanar 平面 mat_c GM；其余为四路 Cube 临时 GM
     */
    __aicore__ inline void Init(GM_ADDR matPlanar, GM_ADDR tmpLoEven, GM_ADDR tmpLoOdd, GM_ADDR tmpHiEven,
                                GM_ADDR tmpHiOdd)
    {
        gm_out_.SetGlobalBuffer((__gm__ int32_t *)matPlanar);
        gm_lo_even_.SetGlobalBuffer((__gm__ int32_t *)tmpLoEven);
        gm_lo_odd_.SetGlobalBuffer((__gm__ int32_t *)tmpLoOdd);
        gm_hi_even_.SetGlobalBuffer((__gm__ int32_t *)tmpHiEven);
        gm_hi_odd_.SetGlobalBuffer((__gm__ int32_t *)tmpHiOdd);
        pipe_.InitBuffer(que_limb_tile_, 1, limbTileLength_ * sizeof(int32_t));
    }

    /** 打包本核 poly bank 的 lo 半平面与 hi 半平面 */
    __aicore__ inline void Process()
    {
        packBank(polyBase_, polyCount_);
    }

private:
    /**
     * 对每个 poly：从 even/odd 临时取 hi 行与 lo 行各 halfLen，拼成 4×halfLen limb tile 写入平面。
     * @param half 0=偶半（系数 0..127）/ 1=奇半（128..255）对应的平面 half 维
     */
    __aicore__ inline void packPolyHalf(uint32_t polyBase, uint16_t kPolys, uint32_t half,
                                        AscendC::GlobalTensor<int32_t> &gmEven,
                                        AscendC::GlobalTensor<int32_t> &gmOdd)
    {
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t hiR = polyBase + static_cast<uint32_t>(lp);
            const uint32_t loR = static_cast<uint32_t>(tiling::kK) + polyBase + static_cast<uint32_t>(lp);
            const uint32_t slot = polyBase + static_cast<uint32_t>(lp);
            LocalTensor<int32_t> tile = que_limb_tile_.AllocTensor<int32_t>();
            // tile 布局：[hh | lh | hl | ll] 各 halfLen，与 RouteA Horner 输入一致
            DataCopy(tile[0], gmEven[hiR * halfLen_], halfLen_);
            DataCopy(tile[halfLen_], gmOdd[hiR * halfLen_], halfLen_);
            DataCopy(tile[2 * halfLen_], gmEven[loR * halfLen_], halfLen_);
            DataCopy(tile[3 * halfLen_], gmOdd[loR * halfLen_], halfLen_);
            KYBER_PIPE_ALL();
            const uint32_t dstBase = planar_k8::mat_row(slot, 0U, half) * halfLen_;
            DataCopy(gm_out_[dstBase], tile, limbTileLength_);
            KYBER_PIPE_ALL();
            que_limb_tile_.FreeTensor(tile);
        }
    }

    __aicore__ inline void packBank(uint32_t polyBase, uint16_t kPolys)
    {
        packPolyHalf(polyBase, kPolys, 0U, gm_lo_even_, gm_lo_odd_);
        packPolyHalf(polyBase, kPolys, 1U, gm_hi_even_, gm_hi_odd_);
    }

    int32_t subCoreIdx_;
    uint32_t coeffN_;
    uint32_t halfLen_;
    uint32_t polyBase_;
    uint16_t polyCount_;
    uint32_t limbTileLength_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_limb_tile_;
    AscendC::GlobalTensor<int32_t> gm_out_;
    AscendC::GlobalTensor<int32_t> gm_lo_even_, gm_lo_odd_, gm_hi_even_, gm_hi_odd_;
};

/**
 * Stage3：平面 mat_c → dst [polys,256] int32（RouteA Horner + mod q）。
 *
 * 与 vec-k4-v2 Aiv2s1eRouteAMod 同构；三段式 NTT/INTT 内禁止 Gather。
 * 输入：matPlanar；输出：dst 行主序 poly×N。
 * F203_STAGE3_MOD 选型见 stage3_config.hpp。
 */
class AivK8RouteAMod {
public:
    __aicore__ inline AivK8RouteAMod(int32_t subCoreIdx, uint32_t coeffN)
        : coeffN_(coeffN), halfLen_(coeffN / 2), limbTileLength_(4 * (coeffN / 2)),
          polyBase_(ntt_poly_base(subCoreIdx)), polyCount_(ntt_poly_count(subCoreIdx))
    {
    }

    /**
     * @param dst 合并后系数 GM；@param matPlanar 平面 mat_c
     */
    __aicore__ inline void Init(GM_ADDR dst, GM_ADDR matPlanar)
    {
        gm_dst_.SetGlobalBuffer((__gm__ int32_t *)dst);
        gm_planar_.SetGlobalBuffer((__gm__ int32_t *)matPlanar);
        const uint32_t maxTile = tiling::kPolysPerAiv * coeffN_ * sizeof(int32_t);
        const uint32_t maxPlane = tiling::kPolysPerAiv * limbTileLength_ * sizeof(int32_t);
        pipe_.InitBuffer(out_dst_, 1, maxTile);
        pipe_.InitBuffer(scratch_t1_, 1, halfLen_ * sizeof(int32_t));
#if F203_STAGE3_MOD == 0
        pipe_.InitBuffer(scratch_t2_, 1, halfLen_ * sizeof(int32_t));
#elif F203_STAGE3_MOD == 2
        pipe_.InitBuffer(calc_f_, 3 * halfLen_ * sizeof(float));
#endif
        pipe_.InitBuffer(half_plane_, 1, maxPlane);
        pipe_.InitBuffer(limb_scratch_, 1, limbTileLength_ * sizeof(int32_t));
        pipe_.InitBuffer(tmp_half_, 1, halfLen_ * sizeof(int32_t));
    }

    /** 处理本核 poly bank：先 lo 半系数再 hi 半，写满 [kPolys,N] 后 copyOut */
    __aicore__ inline void Process()
    {
        ProcessBank(polyBase_, polyBase_, polyCount_);
    }

private:
    /**
     * 批量 RouteA：读 lo 半平面 → 每 poly combine → 写 local_dst[0..half)；
     * 再读 hi 半平面 → 写 local_dst[half..N)；最后 GM copyOut。
     */
    __aicore__ inline void ProcessBank(uint32_t slotBase, uint32_t dstPolyOff, uint16_t kPolys)
    {
        const uint32_t batchHalfPlaneLength = static_cast<uint32_t>(kPolys) * limbTileLength_;

        LocalTensor<int32_t> local_dst = out_dst_.AllocTensor<int32_t>();
        LocalTensor<int32_t> t1 = scratch_t1_.AllocTensor<int32_t>();
#if F203_STAGE3_MOD == 0
        LocalTensor<int32_t> t2 = scratch_t2_.AllocTensor<int32_t>();
#elif F203_STAGE3_MOD == 2
        const uint32_t fStride = halfLen_ * static_cast<uint32_t>(sizeof(float));
        LocalTensor<float> fRaw = calc_f_.GetWithOffset<float>(halfLen_, 0U);
        LocalTensor<float> fTmp = calc_f_.GetWithOffset<float>(halfLen_, fStride);
        LocalTensor<float> fQuot = calc_f_.GetWithOffset<float>(halfLen_, 2U * fStride);
#else
        LocalTensor<int32_t> t2 = t1;
#endif

        LocalTensor<int32_t> plane = half_plane_.AllocTensor<int32_t>();
        const uint32_t loPlaneBase = planar_k8::mat_row(slotBase, 0U, 0U) * halfLen_;
        const uint32_t hiPlaneBase = planar_k8::mat_row(slotBase, 0U, 1U) * halfLen_;
        // —— lo 半平面（系数低 halfN）——
        DataCopy(plane, gm_planar_[loPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();

        LocalTensor<int32_t> half_out = tmp_half_.AllocTensor<int32_t>();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN_;
            const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength_;
            LocalTensor<int32_t> limbs = limb_scratch_.AllocTensor<int32_t>();
            DataCopy(limbs, plane[limbOff], limbTileLength_);
            KYBER_PIPE_ALL();
            LocalTensor<int32_t> hh = limbs[0];
            LocalTensor<int32_t> lh = limbs[halfLen_];
            LocalTensor<int32_t> hl = limbs[2 * halfLen_];
            LocalTensor<int32_t> ll = limbs[3 * halfLen_];
#if F203_STAGE3_MOD == 2
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, fRaw, fTmp, fQuot, 3329,
                                         static_cast<int32_t>(halfLen_));
#else
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, t2, 3329, static_cast<int32_t>(halfLen_));
#endif
            KYBER_PIPE_ALL();
            DataCopy(local_dst[outBase], half_out, halfLen_);
            KYBER_PIPE_ALL();
            limb_scratch_.FreeTensor(limbs);
        }

        // —— hi 半平面（系数高 halfN）——
        DataCopy(plane, gm_planar_[hiPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN_;
            const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength_;
            LocalTensor<int32_t> limbs = limb_scratch_.AllocTensor<int32_t>();
            DataCopy(limbs, plane[limbOff], limbTileLength_);
            KYBER_PIPE_ALL();
            LocalTensor<int32_t> hh = limbs[0];
            LocalTensor<int32_t> lh = limbs[halfLen_];
            LocalTensor<int32_t> hl = limbs[2 * halfLen_];
            LocalTensor<int32_t> ll = limbs[3 * halfLen_];
#if F203_STAGE3_MOD == 2
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, fRaw, fTmp, fQuot, 3329,
                                         static_cast<int32_t>(halfLen_));
#else
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, t2, 3329, static_cast<int32_t>(halfLen_));
#endif
            KYBER_PIPE_ALL();
            DataCopy(local_dst[outBase + halfLen_], half_out, halfLen_);
            KYBER_PIPE_ALL();
            limb_scratch_.FreeTensor(limbs);
        }

        tmp_half_.FreeTensor(half_out);
        half_plane_.FreeTensor(plane);
        out_dst_.EnQue(local_dst);
        scratch_t1_.FreeTensor(t1);
#if F203_STAGE3_MOD == 0
        scratch_t2_.FreeTensor(t2);
#endif
        KYBER_PIPE_ALL();
        copyOut(dstPolyOff, kPolys);
    }

    /** UB local_dst → GM dst，按 poly 行写 */
    __aicore__ inline void copyOut(uint32_t dstPolyOff, uint16_t kPolys)
    {
        LocalTensor<int32_t> local_dst = out_dst_.DeQue<int32_t>();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t dstOff = (dstPolyOff + static_cast<uint32_t>(lp)) * coeffN_;
            const uint32_t locOff = static_cast<uint32_t>(lp) * coeffN_;
            DataCopy(gm_dst_[dstOff], local_dst[locOff], coeffN_);
            KYBER_PIPE_ALL();
        }
        out_dst_.FreeTensor(local_dst);
    }

    uint32_t coeffN_;
    uint32_t halfLen_;
    uint32_t limbTileLength_;
    uint32_t polyBase_;
    uint16_t polyCount_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t1_;
#if F203_STAGE3_MOD == 0
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t2_;
#elif F203_STAGE3_MOD == 2
    AscendC::TBuf<AscendC::TPosition::VECCALC> calc_f_;
#endif
    AscendC::TQue<AscendC::TPosition::VECIN, 1> half_plane_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> limb_scratch_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> tmp_half_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst_;
    AscendC::GlobalTensor<int32_t> gm_planar_, gm_dst_;
};

#endif
