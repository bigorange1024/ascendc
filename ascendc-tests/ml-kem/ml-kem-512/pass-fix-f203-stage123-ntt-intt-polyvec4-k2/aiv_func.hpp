/**
 * @file aiv_func.hpp
 * @brief Tag5T 4-poly 向量侧：Stage1 紧凑 split、Stage2 后平面 pack、Stage3 RouteA merge/mod。
 *
 * 流水线位置（MIX 1×AIC + 2×AIV，见 mmad_custom.cpp）：
 *   1. AivK4Split：src [4,256] int32 → S0 [8,256] int8（[HI₄,LO₄]，每项仍是 6-bit limb）；
 *   2. AIC 四路 MMAD → mat_c_tmp；
 *   3. AivK4PackMatCPlanar：四临时 → 平面 mat_c [32,128] int32；
 *   4. AivK4RouteAMod：平面 → dst [4,256] int32（Horner+mod，无 Gather）。
 *
 * 语义（强制）：
 *   - poly-batch：AIV0 连续处理 poly {0,1}，AIV1 连续处理 poly {2,3}；每核握有完整 poly 的 HI+LO；
 *   - 平面 mat_c：每 slot 四 limb × 两 half（lo/hi 半系数）；
 *   - 三段式内禁止 Gather。
 *
 * 与 golden 关系：s0 / mat_c / dst 分别对应 golden_s0、golden_mat_c、golden_dst（gen_data）。
 */
#ifndef STAGE123_POLYVEC4_AIV_FUNC_HPP
#define STAGE123_POLYVEC4_AIV_FUNC_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "ntt_vec.hpp"
#include "stage1_config.hpp"
#include "stage3_config.hpp"
#include "tiling.h"

using AscendC::DataCopy;

namespace planar_k4 {

/**
 * 平面 mat_c 行号：half∈{0,1}（lo/hi 半）、slot∈[0,4)、limb∈[0,4)。
 * 布局：先 half 大块（各 16 行），块内按 slot 连续 4 limb 行。
 *
 * @return 行索引 ∈ [0, 32)
 */
__aicore__ inline uint32_t mat_row(uint32_t slot, uint32_t limb, uint32_t half)
{
    return half * static_cast<uint32_t>(tiling::kPlanarSlots) * tiling::kLimbsPerPoly +
           slot * tiling::kLimbsPerPoly + limb;
}

} // namespace planar_k4

/**
 * Stage1：k=4 紧凑 [HI, LO] → S0 int8 [8,256]。
 *
 * 输入：GM src 中本 AIV 的 2 条 poly（polyBase..polyBase+1），各 256 int32。
 * 输出：GM S0 行 polyBase.. 写 HI，行 K+polyBase.. 写 LO。
 * 前置：subCoreIdx 0/1 决定 polyBase；仅 AIV 调用。
 */
class AivK4Split {
public:
    /**
     * @param subCoreIdx  AIV 子核号 0 或 1
     * @param coeffN      单 poly 系数数（256）
     */
    __aicore__ inline AivK4Split(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN),
          polyBase_((subCoreIdx == 0) ? 0U : static_cast<uint32_t>(tiling::kPolysPerAiv))
    {
    }

    /**
     * 绑定 S0 / src GM，并按 bank 尺寸分配 VECIN/VECOUT（及向量 split scratch）。
     * @param wsS0  workspace+S0
     * @param src   输入 polyvec GM
     */
    __aicore__ inline void Init(GM_ADDR wsS0, GM_ADDR src)
    {
        gm_src_.SetGlobalBuffer((__gm__ int32_t *)src);
        gm_s0_.SetGlobalBuffer((__gm__ int8_t *)wsS0);
        const uint32_t inBytes = tiling::kPolysPerAiv * coeffN_ * sizeof(int32_t);
        const uint32_t outBytes = tiling::kPolysPerAiv * coeffN_ * sizeof(int8_t);
        pipe_.InitBuffer(inQ_, 1, inBytes);
        pipe_.InitBuffer(out0_, 1, outBytes);
        pipe_.InitBuffer(out1_, 1, outBytes);
#if F203_STAGE1_SPLIT >= 1
        // 向量 split：3×int32 + int16 + half，按整 bank 最大长度
        const uint32_t maxBank = tiling::kPolysPerAiv * coeffN_;
        const uint32_t scratchBytes = maxBank * (3U * sizeof(int32_t) + sizeof(int16_t) + sizeof(half));
        pipe_.InitBuffer(splitScratch_, scratchBytes);
#endif
    }

    /** 对本 AIV 的 2-poly bank 执行 encodeBank */
    __aicore__ inline void Process()
    {
        encodeBank(polyBase_, static_cast<uint16_t>(tiling::kPolysPerAiv));
    }

private:
    /**
     * 读入 [polyBase, polyBase+kPolys) 的 int32，split 为 lo/hi int8，写回 S0 紧凑行。
     *
     * 索引：hiRow = polyBase+lp；loRow = K + polyBase+lp（K=4）。
     * local_dst0=lo，local_dst1=hi（与 Tensor_int8x4 约定一致）。
     */
    __aicore__ inline void encodeBank(uint32_t polyBase, uint16_t kPolys)
    {
        const uint32_t srcOff = polyBase * coeffN_;
        const uint32_t elemCount = static_cast<uint32_t>(kPolys) * coeffN_;

        // GM → UB：本核 2×256 int32，保持完整 poly 粒度而非 hi/lo limbsplit
        LocalTensor<int32_t> local_src = inQ_.AllocTensor<int32_t>();
        DataCopy(local_src, gm_src_[srcOff], elemCount);
        KYBER_PIPE_ALL();
        inQ_.EnQue(local_src);
        local_src = inQ_.DeQue<int32_t>();

        LocalTensor<int8_t> local_dst0 = out0_.AllocTensor<int8_t>();
        LocalTensor<int8_t> local_dst1 = out1_.AllocTensor<int8_t>();
        Tensor_int8x4 res{local_dst0, local_dst1};
#if F203_STAGE1_SPLIT >= 1
        // 向量路径：tileLen=32（bulk 时忽略）
        split_vec(res, local_src, static_cast<int32_t>(elemCount), splitScratch_, 32);
#else
        split_vec(res, local_src, static_cast<int32_t>(elemCount));
#endif
        out0_.EnQue(local_dst0);
        out1_.EnQue(local_dst1);
        inQ_.FreeTensor(local_src);

        local_dst0 = out0_.DeQue<int8_t>();
        local_dst1 = out1_.DeQue<int8_t>();
        // 按 poly 写回：HI 在上半 4 行，LO 在下半 4 行
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t hiRow = polyBase + static_cast<uint32_t>(lp);
            const uint32_t loRow = static_cast<uint32_t>(tiling::kK) + polyBase + static_cast<uint32_t>(lp);
            const uint32_t locOff = static_cast<uint32_t>(lp) * coeffN_;
            DataCopy(gm_s0_[hiRow * coeffN_], local_dst1[locOff], coeffN_);
            DataCopy(gm_s0_[loRow * coeffN_], local_dst0[locOff], coeffN_);
            KYBER_PIPE_ALL();
        }
        out0_.FreeTensor(local_dst0);
        out1_.FreeTensor(local_dst1);
    }

    int32_t subCoreIdx_;
    uint32_t coeffN_;
    uint32_t polyBase_;
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
 * Stage2 后：Cube 四路临时 → 平面 mat_c [32,128]。
 *
 * 对每个 poly slot，将 HI/LO 行在 even/odd 临时中的半列，按 limb 顺序写入平面：
 *   limb0=hh(even from HI), limb1=lh(odd from HI), limb2=hl(even from LO), limb3=ll(odd from LO)；
 * half=0 用 LO_* 临时，half=1 用 HI_* 临时（命名与 LUT top/bottom 对应）。
 *
 * 前置：AIC 四次 Process 已完成；仅 AIV、poly-batch 切分。
 */
class AivK4PackMatCPlanar {
public:
    __aicore__ inline AivK4PackMatCPlanar(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN), halfLen_(coeffN / 2),
          polyBase_((subCoreIdx == 0) ? 0U : static_cast<uint32_t>(tiling::kPolysPerAiv)),
          limbTileLength_(4 * (coeffN / 2))
    {
    }

    /**
     * @param matPlanar  平面 mat_c GM
     * @param tmpLoEven..tmpHiOdd  四路 Cube 临时 GM
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

    __aicore__ inline void Process()
    {
        packBank(polyBase_, static_cast<uint16_t>(tiling::kPolysPerAiv));
    }

private:
    /**
     * 对 half∈{0,1}：遍历本核 kPolys 个 slot，从 gmEven/gmOdd 的 HI/LO 行拼 4×halfLen 写入平面。
     *
     * tile 布局：[0]=HI×even，[halfLen]=HI×odd，[2*halfLen]=LO×even，[3*halfLen]=LO×odd。
     * dstBase = mat_row(slot,0,half) * halfLen。
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
            // 从 Cube 临时按 S0 行号取半列（halfLen=128）
            DataCopy(tile[0], gmEven[hiR * halfLen_], halfLen_);
            DataCopy(tile[halfLen_], gmOdd[hiR * halfLen_], halfLen_);
            DataCopy(tile[2 * halfLen_], gmEven[loR * halfLen_], halfLen_);
            DataCopy(tile[3 * halfLen_], gmOdd[loR * halfLen_], halfLen_);
            KYBER_PIPE_ALL();
            const uint32_t dstBase = planar_k4::mat_row(slot, 0U, half) * halfLen_;
            DataCopy(gm_out_[dstBase], tile, limbTileLength_);
            KYBER_PIPE_ALL();
            que_limb_tile_.FreeTensor(tile);
        }
    }

    /** 先 pack half=0（LO 临时），再 half=1（HI 临时） */
    __aicore__ inline void packBank(uint32_t polyBase, uint16_t kPolys)
    {
        packPolyHalf(polyBase, kPolys, 0U, gm_lo_even_, gm_lo_odd_);
        packPolyHalf(polyBase, kPolys, 1U, gm_hi_even_, gm_hi_odd_);
    }

    int32_t subCoreIdx_;
    uint32_t coeffN_;
    uint32_t halfLen_;
    uint32_t polyBase_;
    uint32_t limbTileLength_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_limb_tile_;
    AscendC::GlobalTensor<int32_t> gm_out_;
    AscendC::GlobalTensor<int32_t> gm_lo_even_, gm_lo_odd_, gm_hi_even_, gm_hi_odd_;
};

/**
 * Stage3：平面 mat_c → dst [4,256]（与 vec-k4-v2 AivK4RouteAMod 同构）。
 *
 * 对每个 poly：先合并 half=0 四 limb → 系数 [0,128)，再 half=1 → [128,256)；
 * 使用 combine_limb6_routea_mod_vec（默认 Barrett），q=3329。
 *
 * 前置：平面已由 Pack 写好；无 Gather；poly-batch 切分同 Stage1。
 */
class AivK4RouteAMod {
public:
    __aicore__ inline AivK4RouteAMod(int32_t subCoreIdx, uint32_t coeffN)
        : coeffN_(coeffN), halfLen_(coeffN / 2), limbTileLength_(4 * (coeffN / 2)),
          polyBase_((subCoreIdx == 0) ? 0U : static_cast<uint32_t>(tiling::kPolysPerAiv))
    {
    }

    /**
     * 分配输出、平面批缓冲、limb scratch、mod 临时（随 F203_STAGE3_MOD）。
     * @param dst        输出 GM [4,256]
     * @param matPlanar  平面 mat_c
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

    __aicore__ inline void Process()
    {
        ProcessBank(polyBase_, polyBase_, static_cast<uint16_t>(tiling::kPolysPerAiv));
    }

private:
    /**
     * 合并本核 kPolys 个 slot：先 lo 半平面，再 hi 半平面，结果拼成完整 poly 后 copyOut。
     *
     * @param slotBase    平面 slot 起始（=polyBase）
     * @param dstPolyOff  dst 中 poly 起始行（同 polyBase）
     * @param kPolys      本核条数（2）
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

        // ---- half=0：系数低半 [0,128) ----
        LocalTensor<int32_t> plane = half_plane_.AllocTensor<int32_t>();
        const uint32_t loPlaneBase = planar_k4::mat_row(slotBase, 0U, 0U) * halfLen_;
        const uint32_t hiPlaneBase = planar_k4::mat_row(slotBase, 0U, 1U) * halfLen_;
        DataCopy(plane, gm_planar_[loPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();

        LocalTensor<int32_t> half_out = tmp_half_.AllocTensor<int32_t>();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN_;
            const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength_;
            LocalTensor<int32_t> limbs = limb_scratch_.AllocTensor<int32_t>();
            DataCopy(limbs, plane[limbOff], limbTileLength_);
            KYBER_PIPE_ALL();
            // 四 limb 视图：hh/lh/hl/ll 各 halfLen
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

        // ---- half=1：系数高半 [128,256) ----
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

    /**
     * 将 UB 中 kPolys 条完整 poly 写回 GM dst。
     * @param dstPolyOff  起始 poly 下标
     * @param kPolys      条数
     */
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
