// @probe exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4
// @file aiv_func.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `aiv_func.hpp` 为该子模块组件。 / Component: aiv_func.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: basic.hpp, kernel_operator.h, kyber_limb6.hpp, ntt_vec.hpp, stage1_config.hpp, stage3_config.hpp, tiling.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


#ifndef NTTS_2S1E_AIV_FUNC_HPP
#define NTTS_2S1E_AIV_FUNC_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "ntt_vec.hpp"
#include "stage1_config.hpp"
#include "stage3_config.hpp"
#include "tiling.h"

using AscendC::DataCopy;

/**
 * @file aiv_func.hpp
 * @brief KeyGen Launch 2 — AIV 侧 Stage1 拆分、Stage2 平面 pack、独立 Stage3。
 *
 * ## 流水线位置
 * Tag5T NTT：S1 limb6 编码 → S2 后平面 mat_c →（调试）独立 S3 merge/mod。
 * 生产路径 S3+行18–20 在 `Aiv2s1eUbPipeline`（2s1e_post_ntt_ub.hpp）内联融合。
 *
 * ## 对齐与 golden
 * FIPS 203 Alg.13 / ML-KEM-1024（k=4）；平面行号与 gen_data.py::planar_row 一致；
 * 验收仅 I/O 等价。NTT S1–S3 **禁止 Gather**，一律 bulk DataCopy。
 *
 * ## 类职责
 *
 * | 类 | 阶段 | 说明 |
 * |----|------|------|
 * | `Aiv2s1eSplit` | S1 | host src → limb6 int8 S0；ŝ 双份、ê 对半 |
 * | `Aiv2s1ePackMatCPlanar` | S2 后 | Cube 四路临时 → 平面 mat_c [96,128] |
 * | `Aiv2s1eRouteAMod` | S3 独立 | 平面 merge+mod → dst（生产用 UbPipeline） |
 */
class Aiv2s1eSplit {
public:
    /** @param subCoreIdx 0=AIV0, 1=AIV1；决定读 src 哪段、写 S0 哪行块 */
    __aicore__ inline Aiv2s1eSplit(int32_t subCoreIdx, uint32_t coeffN) : subCoreIdx_(subCoreIdx), coeffN_(coeffN) {}

    /**
     * 绑定 S0 / src GM 并分配 VECIN/VECOUT（及可选 split scratch）。
     * @param wsS0 workspace 内 S0 基址（int8 行块）
     * @param src  prep 输出的 ŝ‖ê [8,256] int32
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
#if F203_STAGE1_SPLIT == 1
        // 整 bank 向量拆分：scratch 覆盖 maxBank 个系数的中间 dtype
        const uint32_t maxBank = static_cast<uint32_t>(tiling::kPolysPerAiv) * coeffN_;
        const uint32_t scratchBytes = maxBank * (3U * sizeof(int32_t) + sizeof(int16_t) + sizeof(half));
        pipe_.InitBuffer(splitScratch_, scratchBytes);
#elif F203_STAGE1_SPLIT == 2
        // 分 tile 拆分：scratch 仅 kSplitTileLen_ 宽
        const uint32_t tl = static_cast<uint32_t>(kSplitTileLen_);
        const uint32_t scratchBytes = tl * (3U * sizeof(int32_t) + sizeof(int16_t) + sizeof(half));
        pipe_.InitBuffer(splitScratch_, scratchBytes);
#endif
    }

    /**
     * 按 AIV 角色编码：AIV0 写 ŝ 行块 S0_ROW_S0 + ê 前半；AIV1 写 ŝ 副本 + ê 后半。
     * 背景：poly-batch 语义要求每 AIV 握有完整 ŝ；ê 对半分核。
     */
    __aicore__ inline void Process()
    {
        // ŝ：两 AIV 各写一份完整 k 个 poly（不同 S0 行基址）
        const uint32_t sRowBase =
            (subCoreIdx_ == 0) ? static_cast<uint32_t>(tiling::S0_ROW_S0) : static_cast<uint32_t>(tiling::S0_ROW_S1);
        encodeBank(0U, sRowBase, static_cast<uint16_t>(tiling::kPolysPerAiv));

        // ê：AIV0 读 src 行 4..5，AIV1 读 6..7；写到 S0_ROW_E0/E1
        const uint32_t eSrcOff =
            (subCoreIdx_ == 0) ? static_cast<uint32_t>(tiling::kS) * coeffN_
                               : static_cast<uint32_t>(tiling::kS + tiling::kEPerAiv) * coeffN_;
        const uint32_t eRowBase =
            (subCoreIdx_ == 0) ? static_cast<uint32_t>(tiling::S0_ROW_E0) : static_cast<uint32_t>(tiling::S0_ROW_E1);
        encodeBank(eSrcOff, eRowBase, static_cast<uint16_t>(tiling::kEPerAiv));
    }

private:
    /**
     * int32 poly bank → limb6 int8：hi 行 rowBase..，lo 行 rowBase+kPolys..。
     * @param srcOff  gm_src_ 元素偏移
     * @param rowBase S0 起始行
     * @param kPolys  本 bank poly 数
     */
    __aicore__ inline void encodeBank(uint32_t srcOff, uint32_t rowBase, uint16_t kPolys)
    {
        const uint32_t elemCount = static_cast<uint32_t>(kPolys) * coeffN_;

        // GM→UB：整 bank int32
        LocalTensor<int32_t> local_src = inQ_.AllocTensor<int32_t>();
        DataCopy(local_src, gm_src_[srcOff], elemCount);
        KYBER_PIPE_ALL();
        inQ_.EnQue(local_src);
        local_src = inQ_.DeQue<int32_t>();

        // split_vec：lo→local_dst0，hi→local_dst1（limb6）
        LocalTensor<int8_t> local_dst0 = out0_.AllocTensor<int8_t>();
        LocalTensor<int8_t> local_dst1 = out1_.AllocTensor<int8_t>();
        Tensor_int8x4 res{local_dst0, local_dst1};
#if F203_STAGE1_SPLIT >= 1
        split_vec(res, local_src, static_cast<int32_t>(elemCount), splitScratch_, kSplitTileLen_);
#else
        split_vec(res, local_src, static_cast<int32_t>(elemCount));
#endif
        out0_.EnQue(local_dst0);
        out1_.EnQue(local_dst1);
        inQ_.FreeTensor(local_src);

        // 按 poly 写回：hi 行用 dst1，lo 行用 dst0（与 Cube 左矩阵行约定一致）
        local_dst0 = out0_.DeQue<int8_t>();
        local_dst1 = out1_.DeQue<int8_t>();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t hiRow = rowBase + static_cast<uint32_t>(lp);
            const uint32_t loRow = rowBase + static_cast<uint32_t>(kPolys) + static_cast<uint32_t>(lp);
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
    static constexpr int32_t kSplitTileLen_ = 32;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out0_, out1_;
#if F203_STAGE1_SPLIT >= 1
    AscendC::TBuf<AscendC::TPosition::VECCALC> splitScratch_;
#endif
    AscendC::GlobalTensor<int32_t> gm_src_;
    AscendC::GlobalTensor<int8_t> gm_s0_;
};

namespace planar2s1e {

/**
 * 平面 mat_c 行号：half×(slots×limbs) + slot×limbs + limb。
 * 与 tiling.h / gen_data.py::planar_row 一致；S1–S3 禁止 Gather，靠此公式 bulk 寻址。
 */
__aicore__ inline uint32_t mat_row(uint32_t slot, uint32_t limb, uint32_t half)
{
    return half * static_cast<uint32_t>(tiling::kPlanarSlots) * tiling::kLimbsPerPoly +
           slot * tiling::kLimbsPerPoly + limb;
}

} // namespace planar2s1e

/**
 * Stage2 收尾：Cube 偶/奇列分乘结果 → 平面 mat_c [96,128]（无 Gather）。
 * 每个 poly×half 拼四 limb 行：hh/lh/hl/ll，供 Stage3 RouteA merge。
 */
class Aiv2s1ePackMatCPlanar {
public:
    __aicore__ inline Aiv2s1ePackMatCPlanar(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN), halfLen_(coeffN / 2),
          limbTileLength_(4 * (coeffN / 2))
    {
    }

    /**
     * 绑定平面输出与四路 Cube 临时 GM。
     * @param matPlanar 平面 mat_c 基址
     * @param tmpLo/Hi × Even/Odd  四路竖堆临时（AIC 写出）
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

    /** 按 AIV 角色 pack ŝ 副本槽 + ê 半槽到平面布局 */
    __aicore__ inline void Process()
    {
        if (subCoreIdx_ == 0) {
            packBank(static_cast<uint32_t>(tiling::S0_ROW_S0), static_cast<uint32_t>(tiling::PLANAR_SLOT_S0),
                     static_cast<uint16_t>(tiling::kS));
            packBank(static_cast<uint32_t>(tiling::S0_ROW_E0), static_cast<uint32_t>(tiling::PLANAR_SLOT_E0),
                     static_cast<uint16_t>(tiling::kEPerAiv));
        } else {
            packBank(static_cast<uint32_t>(tiling::S0_ROW_S1), static_cast<uint32_t>(tiling::PLANAR_SLOT_S1),
                     static_cast<uint16_t>(tiling::kS));
            packBank(static_cast<uint32_t>(tiling::S0_ROW_E1), static_cast<uint32_t>(tiling::PLANAR_SLOT_E1),
                     static_cast<uint16_t>(tiling::kEPerAiv));
        }
    }

private:
    /**
     * 单 half：从 Cube 临时 even/odd 列拼四 limb → 平面 mat_c 连续 4 行。
     * tile 布局：[hh_even | hh_odd | ll_even | ll_odd] 各 halfLen_（实际为 hi/lo×even/odd）。
     */
    __aicore__ inline void packPolyHalf(uint32_t rowBase, uint32_t slotBase, uint16_t kPolys, uint32_t half,
                                        AscendC::GlobalTensor<int32_t> &gmEven,
                                        AscendC::GlobalTensor<int32_t> &gmOdd)
    {
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t hiR = rowBase + static_cast<uint32_t>(lp);
            const uint32_t loR = rowBase + static_cast<uint32_t>(kPolys) + static_cast<uint32_t>(lp);
            const uint32_t slot = slotBase + static_cast<uint32_t>(lp);
            LocalTensor<int32_t> tile = que_limb_tile_.AllocTensor<int32_t>();
            // 四段 DataCopy：hi-even、hi-odd、lo-even、lo-odd → 连续 UB tile
            DataCopy(tile[0], gmEven[hiR * halfLen_], halfLen_);
            DataCopy(tile[halfLen_], gmOdd[hiR * halfLen_], halfLen_);
            DataCopy(tile[2 * halfLen_], gmEven[loR * halfLen_], halfLen_);
            DataCopy(tile[3 * halfLen_], gmOdd[loR * halfLen_], halfLen_);
            KYBER_PIPE_ALL();
            const uint32_t dstBase = planar2s1e::mat_row(slot, 0U, half) * halfLen_;
            DataCopy(gm_out_[dstBase], tile, limbTileLength_);
            KYBER_PIPE_ALL();
            que_limb_tile_.FreeTensor(tile);
        }
    }

    /** 先 pack lo half（half=0），再 hi half（half=1） */
    __aicore__ inline void packBank(uint32_t rowBase, uint32_t slotBase, uint16_t kPolys)
    {
        packPolyHalf(rowBase, slotBase, kPolys, 0U, gm_lo_even_, gm_lo_odd_);
        packPolyHalf(rowBase, slotBase, kPolys, 1U, gm_hi_even_, gm_hi_odd_);
    }

    int32_t subCoreIdx_;
    uint32_t coeffN_;
    uint32_t halfLen_;
    uint32_t limbTileLength_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_limb_tile_;
    AscendC::GlobalTensor<int32_t> gm_out_;
    AscendC::GlobalTensor<int32_t> gm_lo_even_, gm_lo_odd_, gm_hi_even_, gm_hi_odd_;
};

/**
 * Stage3（独立调试路径）：平面 mat_c bulk DataCopy 四 limb → RouteA merge+mod → dst。
 * 生产路径请用 Aiv2s1eUbPipeline，本类保留供 mixPass 分段对照。
 */
class Aiv2s1eRouteAMod {
public:
    __aicore__ inline Aiv2s1eRouteAMod(uint32_t coeffN) : coeffN_(coeffN), halfLen_(coeffN / 2),
                                                         limbTileLength_(4 * (coeffN / 2))
    {
    }

    /**
     * @param dst       输出 NTT 系数 GM [polys,256] int32
     * @param matPlanar 平面 mat_c GM
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

    /**
     * 平面 slot 批量 merge+mod → local_dst，再 copyOut 到 gm_dst。
     * @param slotBase   平面槽起始（ŝ/ê 槽）
     * @param dstPolyOff dst 中 poly 行偏移
     * @param kPolys     本 bank poly 数
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
        const uint32_t loPlaneBase = planar2s1e::mat_row(slotBase, 0U, 0U) * halfLen_;
        const uint32_t hiPlaneBase = planar2s1e::mat_row(slotBase, 0U, 1U) * halfLen_;
        // --- 先处理 lo half（系数 0..127）---
        DataCopy(plane, gm_planar_[loPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();

        LocalTensor<int32_t> half_out = tmp_half_.AllocTensor<int32_t>();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN_;
            const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength_;
            LocalTensor<int32_t> limbs = limb_scratch_.AllocTensor<int32_t>();
            DataCopy(limbs, plane[limbOff], limbTileLength_);
            KYBER_PIPE_ALL();
            // hh/lh/hl/ll：四 limb 半行，RouteA 合并后 mod q=3329
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

        // --- 再处理 hi half（系数 128..255），写入 local_dst 后半 ---
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

private:
    /** UB 中完整 poly → GM dst 对应行 */
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
