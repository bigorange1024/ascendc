#ifndef NTTS_2S1E_POST_NTT_UB_HPP
#define NTTS_2S1E_POST_NTT_UB_HPP

#include "aiv_func.hpp"
#include "basic.hpp"
#include "byte_encode12_config.hpp"
#include "alg11_vec_pipe.hpp"
#include "byte_encode12_pair.hpp"
#include "hat_dot_layout.hpp"
#include "hat_dot_ub_tiling.hpp"
#include "hat_alg11_basemul.hpp"
#include "hat_gammas.hpp"
#include "hat_line18_2s1e.hpp"
#include "hat_vec.hpp"
#include "innerproduct_mod.hpp"
#include "integration_config.hpp"
#include "kernel_operator.h"
#include "pipeline_probe.hpp"
#include "mod_variants.hpp"
#include "multiply_ntts_ub.hpp"
#include "ntt_vec.hpp"
#include "stage3_config.hpp"
#include "tiling.h"

using AscendC::DataCopy;

/**
 * @file 2s1e_post_ntt_ub.hpp
 * @brief Aiv2s1eUbPipeline：S3 平面 merge → 行 18 j→p 内积 → 行 19–20 ByteEncode。
 *
 * 单 TPipe（HAT_LINE18_FULLPOLY=1）：
 *   ubNttBuf_   — ŝ[0..3] + ê 本核 2 poly
 *   ubThatBuf_  — t̂ 本核 2 行
 *   dotScratchBuf_ + ROM Queues — 行 18 compute_on_ub
 *
 * 行 18 数学（每个 p）：
 *   lineP = Σ_j NTT-Mul(â[p,j], ŝ[j])  // j 循环内不 final mod
 *   lineP += ê[p]                      // HAT_LINE18_DOT_ONLY=0
 *   lineP = mod_q(lineP)               // 一次 Barrett final mod
 *
 * Golden：golden_t_hat_dot（无 ê）/ golden_t_hat（Σ+ê 后 mod）。
 * 禁止：嵌套第二 TPipe、S3 后 ŝ GM 绕路、SHAT_PEER。
 */
class Aiv2s1eUbPipeline {
public:
    /**
     * 构造：记录本 AIV 的 p 区间、平面 slot 基址与各 UB 长度常量。
     * @param subCoreIdx 0/1；@param coeffN 通常 256
     */
    __aicore__ inline Aiv2s1eUbPipeline(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN), halfLen_(coeffN / 2), pairCount_(halfLen_ / 2),
          kPolysS_(static_cast<uint16_t>(tiling::kS)),
          kPolysE_(static_cast<uint16_t>(tiling::kEPerAiv)),
          kUbPolys_(static_cast<uint16_t>(tiling::kS + tiling::kEPerAiv)),
          limbTileLength_(4 * (coeffN / 2)),
          sBatchPlaneLength_(static_cast<uint32_t>(tiling::kS) * limbTileLength_),
          eBatchPlaneLength_(static_cast<uint32_t>(tiling::kEPerAiv) * limbTileLength_),
          ubNttLength_(static_cast<uint32_t>(tiling::kS + tiling::kEPerAiv) * coeffN),
          aHatTileLength_(static_cast<uint32_t>(tiling::kHatK) * coeffN),
          aHatAivTileLength_(static_cast<uint32_t>(tiling::kEPerAiv) * static_cast<uint32_t>(tiling::kHatK) * coeffN),
          thatTileLength_(static_cast<uint32_t>(tiling::kEPerAiv) * coeffN),
          pBegin_(twos1e::p_begin(subCoreIdx)), pEnd_(twos1e::p_end(subCoreIdx)),
          sSlotBase_((subCoreIdx == 0) ? static_cast<uint32_t>(tiling::PLANAR_SLOT_S0)
                                       : static_cast<uint32_t>(tiling::PLANAR_SLOT_S1)),
          eSlotBase_((subCoreIdx == 0) ? static_cast<uint32_t>(tiling::PLANAR_SLOT_E0)
                                       : static_cast<uint32_t>(tiling::PLANAR_SLOT_E1))
    {
    }

    /**
     * 绑定 GM 并按 HAT_LINE18_FULLPOLY / BYTE_ENCODE 分配单 TPipe UB。
     * @param matPlanar 平面 mat_c；@param a_hat Â[16,256]；@param ek_out/sk_out ByteEncode 输出
     * @param dst_dump NTT dump / 预设；@param t_dump t̂ dump / 预设
     */
    __aicore__ inline void Init(GM_ADDR matPlanar, GM_ADDR a_hat, GM_ADDR ek_out, GM_ADDR sk_out, GM_ADDR dst_dump,
                                GM_ADDR t_dump)
    {
        /* GM 绑定：平面 mat_c、a_hat[16,256]、ek/sk 输出、对拍 dump dst/t_hat */
        gm_planar_.SetGlobalBuffer((__gm__ int32_t *)matPlanar);
        gm_a_.SetGlobalBuffer((__gm__ int32_t *)a_hat, static_cast<uint32_t>(tiling::kHatKK) * coeffN_);
        gm_ek_.SetGlobalBuffer((__gm__ uint8_t *)ek_out);
        gm_sk_.SetGlobalBuffer((__gm__ uint8_t *)sk_out);
        gm_dst_dump_.SetGlobalBuffer((__gm__ int32_t *)dst_dump, static_cast<uint32_t>(tiling::kDstPolys) * coeffN_);
        gm_t_dump_.SetGlobalBuffer((__gm__ int32_t *)t_dump, static_cast<uint32_t>(tiling::kHatK) * coeffN_);

        /*
         * UB 布局（HAT_LINE18_FULLPOLY=1）：
         *   ubNttBuf_     — ŝ[0..3] + ê 本核 2 行
         *   ubThatBuf_    — t̂ 本核 2 行
         *   dotScratchBuf_— fLoc|row|modT2|outLine（hat_dot_ub_tiling）
         *   scratch_      — S3 merge 临时 + encode 字节区
         *   ROM Queues    — γ / gather / interleave（Init 填一次）
         */
        const uint32_t pc = pairCount_;
        const uint32_t hl = halfLen_;
        const uint32_t i32 = sizeof(int32_t);
        const uint32_t u8 = sizeof(uint8_t);
        const uint32_t hatPcMult =
#if HAT_ALG11_VEC >= 1
            hat_alg11_cfg::kHatPcMult;
#else
            10U;
#endif
        uint32_t scratchBytes;
#if HAT_LINE18_FULLPOLY >= 1
        /* S3 merge：plane @ ubNtt+thatTile；limbs @ +sBatch+eBatch+3*half。 */
        scratchBytes = (ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ + 3U * hl +
                        limbTileLength_) *
                       i32;
#if HAT_BYTE_ENCODE >= 1 && HAT_LINE18_DOT_ONLY < 1
        scratchBytes += 2U * byte_encode12::kAivShardBytes * u8;
#if BYTE_ENCODE12_VEC >= 1 && BYTE_ENCODE12_PREFETCH >= 1
        scratchBytes += byte_encode12::kPrefetchScratchBytes;
#elif BYTE_ENCODE12_VEC >= 1
        scratchBytes += byte_encode12::kVecScratchBytes;
#endif
#endif
#else
        scratchBytes = (ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ +
                        limbTileLength_ + halfLen_ +
                        static_cast<uint32_t>(tiling::kEPerAiv + 1) * coeffN_ + aHatAivTileLength_ +
                        hatPcMult * pc + 4U * hl) *
                       i32;
#if HAT_LINE18_DOT_ONLY < 1
        scratchBytes += 2U * byte_encode12::kAivShardBytes * u8;
#if BYTE_ENCODE12_VEC >= 1 && BYTE_ENCODE12_PREFETCH >= 1
        scratchBytes += byte_encode12::kPrefetchScratchBytes;
#elif BYTE_ENCODE12_VEC >= 1
        scratchBytes += byte_encode12::kVecScratchBytes;
#endif
#endif
#if HAT_ALG11_VEC >= 1 && HAT_LINE18_DOT_ONLY < 1
        scratchBytes += hat_alg11_cfg::kExtraInt32Slots * i32;
#endif
#endif
#if F203_MOD_VARIANT == 2
        scratchBytes += 6U * hl * sizeof(float);
#endif
#if HAT_LINE18_FULLPOLY >= 1
        const int32_t kRomPairs = hat_dot_ub::kRomPairCount;
        pipe_.InitBuffer(ubNttBuf_, ubNttLength_ * sizeof(int32_t));
        pipe_.InitBuffer(ubThatBuf_, thatTileLength_ * sizeof(int32_t));
        pipe_.InitBuffer(dotScratchBuf_, static_cast<uint32_t>(hat_dot_ub::kDotScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(hat_dot_ub::kVecWsInts * sizeof(int32_t)));
        pipe_.InitBuffer(inQueueF_, 1, coeffN_ * sizeof(int32_t));
        pipe_.InitBuffer(inQueueG_, 1, coeffN_ * sizeof(int32_t));
        pipe_.InitBuffer(gammaLutQue_, 1, static_cast<uint32_t>(kRomPairs) * sizeof(int32_t));
        pipe_.InitBuffer(gatherEvenQue_, 1, static_cast<uint32_t>(kRomPairs) * sizeof(int32_t));
        pipe_.InitBuffer(gatherOddQue_, 1, static_cast<uint32_t>(kRomPairs) * sizeof(int32_t));
        pipe_.InitBuffer(interleaveReorderQue_, 1, coeffN_ * sizeof(int32_t));
        pipe_.InitBuffer(scratch_, scratchBytes);
#if !defined(ASCENDC_CPU_DEBUG)
        LocalTensor<int32_t> gammaLocal = gammaLutQue_.AllocTensor<int32_t>();
        dotRomUb_.gammaV = gammaLocal;
        LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.AllocTensor<int32_t>();
        dotRomUb_.gatherEvenByte = gatherEvenLocal;
        dotRomUb_.gatherOddByte = gatherOddLocal;
        dotRomUb_.interleaveReorderByte = interleaveLocal;
        alg11_ub::init_rom_luts_ub(dotRomUb_, kRomPairs);
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
#endif
#else
        pipe_.InitBuffer(scratch_, scratchBytes);
        pipe_.InitBuffer(ubNttBuf_, ubNttLength_ * sizeof(int32_t));
        pipe_.InitBuffer(ubThatBuf_, thatTileLength_ * sizeof(int32_t));
        pipe_.InitBuffer(que_ub_ntt_, 1, ubNttLength_ * sizeof(int32_t));
        pipe_.InitBuffer(que_ub_that_, 1, thatTileLength_ * sizeof(int32_t));
#endif
#if HAT_ALG11_VEC >= 1 && HAT_LINE18_FULLPOLY < 1 && HAT_LINE18_DOT_ONLY < 1
        alg11Base_ = ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ + limbTileLength_ +
                     3U * halfLen_ + static_cast<uint32_t>(tiling::kEPerAiv + 1) * coeffN_ + aHatAivTileLength_ +
                     hatPcMult * pc + 4U * hl;
        LocalTensor<int32_t> romBase = bufI32(alg11Base_, hat_alg11_cfg::kRomInt32Slots);
        hat_alg11::init_rom_luts(romBase, romUb_, static_cast<int32_t>(pc));
        KYBER_PIPE_ALL();
#elif HAT_ALG11_VEC >= 1
        (void)romUb_;
#endif
    }

    /**
     * @param runS3Stage  true → stageS3Into（平面 merge+mod → ub_ntt）
     * @param runHat        true → stageHatDotOnly（行 18）
     * @param runEncode     true → stageEncodeOut（行 19–20，需 HAT_BYTE_ENCODE）
     * @param loadNttPreset mixPass 4/7：跳过 S3，从 dst_dump 灌 ŝ/ê
     * @param loadThatPreset mixPass 7：跳过行 18，从 t_dump 灌 t̂
     */
    __aicore__ inline void Process(bool runS3Stage, bool runHat, bool runEncode, bool loadNttPreset,
                                   bool loadThatPreset)
    {
#if HAT_LINE18_FULLPOLY >= 1
        LocalTensor<int32_t> ub_ntt = ubNttBuf_.GetWithOffset<int32_t>(ubNttLength_, 0);
        LocalTensor<int32_t> ub_that = ubThatBuf_.GetWithOffset<int32_t>(thatTileLength_, 0);
#else
        LocalTensor<int32_t> ub_ntt = que_ub_ntt_.AllocTensor<int32_t>();
        LocalTensor<int32_t> ub_that = que_ub_that_.AllocTensor<int32_t>();
#endif

        if (loadNttPreset) {
            loadNttPresetInto(ub_ntt);
#if HAT_LINE18_FULLPOLY >= 1
            KYBER_PIPE_ALL();
#endif
        } else if (runS3Stage) {
            stageS3Into(ub_ntt);
#if HAT_LINE18_FULLPOLY >= 1
            KYBER_PIPE_ALL();
            PIPELINE_PROBE_UB_SAMPLE("after_s3 s_hat j0", ub_ntt, 0U, 8);
            PIPELINE_PROBE_UB_SAMPLE("after_s3 e_hat lp0", ub_ntt, static_cast<uint32_t>(kPolysS_) * coeffN_, 8);
#endif
        }

        if (loadThatPreset) {
            for (uint16_t p = pBegin_; p < pEnd_; ++p) {
                const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * coeffN_;
                DataCopy(ub_that[localP], gm_t_dump_[static_cast<uint32_t>(p) * coeffN_], coeffN_);
                KYBER_PIPE_ALL();
            }
        } else if (runHat) {
            stageHatInto(ub_ntt, ub_that);
        }

        if (runEncode) {
#if HAT_BYTE_ENCODE >= 1 && HAT_LINE18_DOT_ONLY < 1
            stageEncodeOut(ub_ntt, ub_that);
#endif
        }

        /* host 对拍 dump：仅在 S3/行18 计算完成后写 GM，计算路径内 ŝ/t̂ 不离 UB */
        if (runS3Stage && !loadNttPreset) {
            dumpNttDebug(ub_ntt);
        }
        if (runHat && !loadThatPreset) {
            dumpThatDebug(ub_that);
        }

#if HAT_LINE18_FULLPOLY < 1
        que_ub_that_.FreeTensor(ub_that);
        que_ub_ntt_.FreeTensor(ub_ntt);
#endif
        KYBER_PIPE_ALL();
    }

private:
    /** scratch_ 上按 int32 元素偏移取视图：offElems 起 nElems 个。 */
    __aicore__ inline LocalTensor<int32_t> bufI32(uint32_t offElems, uint32_t nElems)
    {
        return scratch_.GetWithOffset<int32_t>(nElems, offElems * static_cast<uint32_t>(sizeof(int32_t)));
    }

    /** mixPass 4/7：从 dst_dump 灌 ub_ntt（ŝ + ê），跳过 S3 */
    __aicore__ inline void loadNttPresetInto(LocalTensor<int32_t> &ub_ntt)
    {
        const uint32_t sDstOff =
            (subCoreIdx_ == 0) ? static_cast<uint32_t>(tiling::dstSOffAiv0) : static_cast<uint32_t>(tiling::dstSOffAiv1);
        const uint32_t eDstOff =
            (subCoreIdx_ == 0) ? static_cast<uint32_t>(tiling::dstEOffAiv0) : static_cast<uint32_t>(tiling::dstEOffAiv1);
        DataCopy(ub_ntt, gm_dst_dump_[sDstOff * coeffN_], static_cast<uint32_t>(kPolysS_) * coeffN_);
        KYBER_PIPE_ALL();
        DataCopy(ub_ntt[static_cast<uint32_t>(kPolysS_) * coeffN_], gm_dst_dump_[eDstOff * coeffN_],
                 static_cast<uint32_t>(kPolysE_) * coeffN_);
        KYBER_PIPE_ALL();
    }

    /** S3：按 slot 读平面 mat_c，limb6 重组 + stage31_mod → ub_ntt 中 ŝ/ê */
    __aicore__ inline void stageS3Into(LocalTensor<int32_t> &ub_ntt)
    {
        mergeBankInto(ub_ntt, 0U, sSlotBase_, kPolysS_, sBatchPlaneLength_);
        mergeBankInto(ub_ntt, static_cast<uint32_t>(kPolysS_), eSlotBase_, kPolysE_, eBatchPlaneLength_);
    }

#if F203_STAGE3_MOD == 2
    /** 平面 batch：先 lo half 全部 poly，再 hi half；scratch 在 ubNtt+that 之后 */
    __aicore__ inline void mergeBankInto(LocalTensor<int32_t> &ub_ntt, uint32_t ubLpBase, uint32_t slotBase,
                                         uint16_t kPolys, uint32_t batchPlaneLength)
    {
        LocalTensor<int32_t> plane = bufI32(ubNttLength_ + thatTileLength_, batchPlaneLength);
        LocalTensor<int32_t> half_out = bufI32(ubNttLength_ + thatTileLength_ + batchPlaneLength, halfLen_);
        LocalTensor<int32_t> t1 = bufI32(ubNttLength_ + thatTileLength_ + batchPlaneLength + halfLen_, halfLen_);
        const uint32_t fBase = ubNttLength_ + thatTileLength_ + batchPlaneLength + 2U * halfLen_;
        const uint32_t fStride = halfLen_ * static_cast<uint32_t>(sizeof(float));
        LocalTensor<float> fRaw = scratch_.GetWithOffset<float>(halfLen_, fBase * sizeof(int32_t));
        LocalTensor<float> fTmp = scratch_.GetWithOffset<float>(halfLen_, fBase * sizeof(int32_t) + fStride);
        LocalTensor<float> fQuot = scratch_.GetWithOffset<float>(halfLen_, fBase * sizeof(int32_t) + 2U * fStride);

        const uint32_t loPlaneBase = planar2s1e::mat_row(slotBase, 0U, 0U) * halfLen_;
        const uint32_t hiPlaneBase = planar2s1e::mat_row(slotBase, 0U, 1U) * halfLen_;
        DataCopy(plane, gm_planar_[loPlaneBase], batchPlaneLength);
        KYBER_PIPE_ALL();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            mergePolyHalf(ub_ntt, plane, half_out, t1, fRaw, fTmp, fQuot, ubLpBase + static_cast<uint32_t>(lp), lp,
                          0U);
        }
        DataCopy(plane, gm_planar_[hiPlaneBase], batchPlaneLength);
        KYBER_PIPE_ALL();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            mergePolyHalf(ub_ntt, plane, half_out, t1, fRaw, fTmp, fQuot, ubLpBase + static_cast<uint32_t>(lp), lp,
                          1U);
        }
        KYBER_PIPE_ALL();
    }

    /**
     * 单 poly 单 half：从 plane 取四 limb → combine_limb6_routea_mod_vec → ub_ntt[ubLp] 的 lo/hi 半。
     * @param ubLp ub_ntt 中 poly 行号；@param lp 本 batch 内 poly 下标；@param halfIdx 0=低半/1=高半
     */
    __aicore__ inline void mergePolyHalf(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &plane,
                                         LocalTensor<int32_t> &half_out, LocalTensor<int32_t> &t1,
                                         LocalTensor<float> &fRaw, LocalTensor<float> &fTmp, LocalTensor<float> &fQuot,
                                         uint32_t ubLp, uint16_t lp, uint32_t halfIdx)
#else
    /** 平面 batch merge（F203_STAGE3_MOD≠2 时用 int32 t2 而非 float Barrett） */
    __aicore__ inline void mergeBankInto(LocalTensor<int32_t> &ub_ntt, uint32_t ubLpBase, uint32_t slotBase,
                                         uint16_t kPolys, uint32_t batchPlaneLength)
    {
        LocalTensor<int32_t> plane = bufI32(ubNttLength_ + thatTileLength_, batchPlaneLength);
        LocalTensor<int32_t> half_out = bufI32(ubNttLength_ + thatTileLength_ + batchPlaneLength, halfLen_);
        LocalTensor<int32_t> t1 = bufI32(ubNttLength_ + thatTileLength_ + batchPlaneLength + halfLen_, halfLen_);
        LocalTensor<int32_t> t2 = bufI32(ubNttLength_ + thatTileLength_ + batchPlaneLength + 2U * halfLen_, halfLen_);

        const uint32_t loPlaneBase = planar2s1e::mat_row(slotBase, 0U, 0U) * halfLen_;
        const uint32_t hiPlaneBase = planar2s1e::mat_row(slotBase, 0U, 1U) * halfLen_;
        DataCopy(plane, gm_planar_[loPlaneBase], batchPlaneLength);
        KYBER_PIPE_ALL();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            mergePolyHalf(ub_ntt, plane, half_out, t1, t2, ubLpBase + static_cast<uint32_t>(lp), lp, 0U);
        }
        DataCopy(plane, gm_planar_[hiPlaneBase], batchPlaneLength);
        KYBER_PIPE_ALL();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            mergePolyHalf(ub_ntt, plane, half_out, t1, t2, ubLpBase + static_cast<uint32_t>(lp), lp, 1U);
        }
        KYBER_PIPE_ALL();
    }

    /**
     * 单 poly 单 half（Barrett/int64 变体）：四 limb merge+mod 写入 ub_ntt。
     * @param t1/t2 int32 临时；其余参数同 float 重载
     */
    __aicore__ inline void mergePolyHalf(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &plane,
                                         LocalTensor<int32_t> &half_out, LocalTensor<int32_t> &t1,
                                         LocalTensor<int32_t> &t2, uint32_t ubLp, uint16_t lp, uint32_t halfIdx)
#endif
    {
        /* hh,lh,hl,ll 四 limb → combine_limb6_routea_mod_vec → ub_ntt[ubLp] 的 lo 或 hi 半 */
        const uint32_t limbBase =
            ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ + 3U * halfLen_;
        LocalTensor<int32_t> limbs = bufI32(limbBase, limbTileLength_);
        const uint32_t outBase = ubLp * coeffN_;
        const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength_;
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
        if (halfIdx == 0U) {
            DataCopy(ub_ntt[outBase], half_out, halfLen_);
        } else {
            DataCopy(ub_ntt[outBase + halfLen_], half_out, halfLen_);
        }
        KYBER_PIPE_ALL();
    }

    /**
     * 行 18 入口：FULLPOLY=1 → stageHatDotOnly；否则 legacy half-row（非生产）。
     * @param ub_ntt ŝ[0..3]+ê；@param ub_that 输出本核 t̂[2,256]
     */
    __aicore__ inline void stageHatInto(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &ub_that)
    {
#if HAT_LINE18_FULLPOLY >= 1
        stageHatDotOnly(ub_ntt, ub_that);
#else
        /** legacy：half-row basemul + 每 half 一次 MOD_Q（frozen v1 路线，勿作生产） */
        stageHatIntoLegacy(ub_ntt, ub_that);
#endif
    }

#if HAT_LINE18_FULLPOLY >= 1
    __aicore__ inline LocalTensor<int32_t> dotBufI32(int32_t offInts, int32_t len)
    {
        return dotScratchBuf_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                                     static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    /**
     * 行 18 生产路径（HAT_LINE18_FULLPOLY=1）。
     * 外循环 j（固定 gPoly=ŝ[j]），内循环 p：累加 alg11 compute_on_ub，最后 +ê、一次 mod_q_final_vec。
     * CPU_DEBUG 分支：标量 alg11 便于 gdb；设备：向量 Add + Barrett mod。
     */
    __aicore__ inline void stageHatDotOnly(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &ub_that)
    {
#if defined(ASCENDC_CPU_DEBUG)
        const int32_t kN = static_cast<int32_t>(coeffN_);
        const int32_t kHatQ = hat_dot_ub::kHatQ;
        const int32_t kSVec = static_cast<int32_t>(tiling::kHatK);
        int32_t prod[256];
        int64_t acc[256];

        for (uint16_t p = pBegin_; p < pEnd_; ++p) {
            for (int32_t c = 0; c < kN; ++c) {
                acc[c] = 0;
            }
            for (int32_t j = 0; j < kSVec; ++j) {
                int32_t fBuf[256];
                int32_t gBuf[256];
                const uint32_t aOff = hat_dot_layout::a_hat_offset(p, static_cast<uint16_t>(j));
                const uint32_t sOff = static_cast<uint32_t>(j) * coeffN_;
                for (int32_t c = 0; c < kN; ++c) {
                    fBuf[c] = gm_a_.GetValue(aOff + static_cast<uint32_t>(c));
                    gBuf[c] = ub_ntt.GetValue(sOff + static_cast<uint32_t>(c));
                }
                alg11_ub::multiply_ntts_scalar(prod, fBuf, gBuf);
                for (int32_t c = 0; c < kN; ++c) {
                    acc[c] += static_cast<int64_t>(prod[c]);
                }
            }
#if HAT_LINE18_DOT_ONLY < 1
            const uint32_t eLp = twos1e::e_local_lp(p, pBegin_);
            for (int32_t c = 0; c < kN; ++c) {
                acc[c] += static_cast<int64_t>(ub_ntt.GetValue(eLp * coeffN_ + static_cast<uint32_t>(c)));
            }
#endif
            const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * coeffN_;
            const int64_t q64 = static_cast<int64_t>(kHatQ);
            for (int32_t c = 0; c < kN; ++c) {
                int64_t rem = acc[c] % q64;
                if (rem < 0) {
                    rem += q64;
                }
                ub_that.SetValue(localP + static_cast<uint32_t>(c), static_cast<int32_t>(rem));
            }
            PIPELINE_PROBE_UB_SAMPLE("after_hat t_hat local", ub_that,
                                     (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * coeffN_, 8);
        }
#else
        const int32_t kN = static_cast<int32_t>(coeffN_);
        const int32_t kPPerAiv = hat_dot_ub::kPPerAiv;
        const int32_t kHatQ = hat_dot_ub::kHatQ;
        const int32_t kSVec = static_cast<int32_t>(tiling::kHatK);

        LocalTensor<int32_t> row = dotBufI32(hat_dot_ub::kOffRow, kN);
        LocalTensor<int32_t> modT2 = dotBufI32(hat_dot_ub::kOffModT2, kN);
        LocalTensor<int32_t> outLine = dotBufI32(hat_dot_ub::kOffOutLine, kPPerAiv * kN);
        LocalTensor<int32_t> fLoc = dotBufI32(hat_dot_ub::kOffFLoc, kN);

        LocalTensor<int32_t> wsLocal = wsQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> gammaLocal = gammaLutQue_.DeQue<int32_t>();
        LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.DeQue<int32_t>();
        LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.DeQue<int32_t>();
        LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.DeQue<int32_t>();
        alg11_vec::RomUbLuts rom;
        rom.gammaV = gammaLocal;
        rom.gatherEvenByte = gatherEvenLocal;
        rom.gatherOddByte = gatherOddLocal;
        rom.interleaveReorderByte = interleaveLocal;

        for (int32_t j = 0; j < kSVec; ++j) {
            LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            DataCopy(gPoly, ub_ntt[static_cast<uint32_t>(j) * coeffN_], static_cast<uint32_t>(kN));
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();

            for (uint16_t p = pBegin_; p < pEnd_; ++p) {
                const uint32_t localP = static_cast<uint32_t>(p - pBegin_);
                const uint32_t lineOff = localP * static_cast<uint32_t>(kN);
                LocalTensor<int32_t> lineP = outLine[lineOff];

                LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
                DataCopy(fPoly, gm_a_[hat_dot_layout::a_hat_offset(p, static_cast<uint16_t>(j))],
                         static_cast<uint32_t>(kN));
                inQueueF_.EnQue(fPoly);
                fPoly = inQueueF_.DeQue<int32_t>();

                alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
                inQueueF_.FreeTensor(fPoly);
                ALG11_PIPE_ALL();

                if (j == 0) {
                    DataCopy(lineP, row, kN);
                    ALG11_PIPE_MTE2();
                } else {
                    AscendC::Add(lineP, lineP, row, kN);
                    ALG11_PIPE_ALL();
                }
            }

            inQueueG_.FreeTensor(gPoly);
        }

        for (uint16_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localP = static_cast<uint32_t>(p - pBegin_) * static_cast<uint32_t>(kN);
            LocalTensor<int32_t> lineP = outLine[localP];
#if HAT_LINE18_DOT_ONLY < 1
            const uint32_t eLp = twos1e::e_local_lp(p, pBegin_);
            AscendC::Add(lineP, lineP, ub_ntt[eLp * coeffN_], static_cast<int32_t>(kN));
            ALG11_PIPE_ALL();
#endif
            hat_ip::mod_q_final_vec(lineP, kHatQ, fLoc, modT2, kN);
            ALG11_PIPE_ALL();
        }

        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);

        DataCopy(ub_that, outLine, static_cast<uint32_t>(kPPerAiv) * coeffN_);
        ALG11_PIPE_MTE2();
#endif
    }
#endif

    /**
     * 行 18 legacy：按 halfLen 切片 basemul，每 half 立即 MOD_Q（与 v2 lazy Σ 不同）。
     * 仅 HAT_LINE18_FULLPOLY=0 编译；勿作生产路径。
     */
    __aicore__ inline void stageHatIntoLegacy(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &ub_that)
    {
        /* halfLen 切片 + multiply_ntts_half_vec；j 外环、subOff 半核；每 half 立即 mod（与 v2 lazy Σ 不同） */
        const uint32_t pc = pairCount_;
        const uint32_t hatBase =
            ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ + limbTileLength_ + 3U * halfLen_;

        LocalTensor<int32_t> acc = bufI32(hatBase, halfLen_);
        LocalTensor<int32_t> row = bufI32(hatBase + halfLen_, halfLen_);
        LocalTensor<int32_t> partial = bufI32(hatBase + 2U * halfLen_, static_cast<uint32_t>(kPolysE_) * coeffN_);
        LocalTensor<int32_t> line = bufI32(hatBase + 2U * halfLen_ + static_cast<uint32_t>(kPolysE_) * coeffN_, coeffN_);
        LocalTensor<int32_t> a_tile =
            bufI32(hatBase + 3U * halfLen_ + static_cast<uint32_t>(kPolysE_) * coeffN_, aHatAivTileLength_);
        const uint32_t aAivBase = static_cast<uint32_t>(pBegin_) * static_cast<uint32_t>(tiling::kHatK) * coeffN_;
        DataCopy(a_tile, gm_a_[aAivBase], aHatAivTileLength_);
        KYBER_PIPE_ALL();

        const uint32_t hatPc =
#if HAT_ALG11_VEC >= 1
            hat_alg11_cfg::kHatPcMult * pc;
#else
            10U * pc;
#endif
#if HAT_ALG11_VEC >= 1
        LocalTensor<int32_t> basemulWs =
            bufI32(alg11Base_ + hat_alg11_cfg::kRomInt32Slots, hat_alg11_cfg::kBasemulWsInts);
        LocalTensor<int32_t> gammaSlice = bufI32(alg11Base_ + hat_alg11_cfg::kRomInt32Slots +
                                                     hat_alg11_cfg::kBasemulWsInts,
                                                 hat_alg11_cfg::kGammaSliceInts);
        alg11_vec::VecWs basemulWsBound;
        hat_alg11::bind_basemul_ws(basemulWs, basemulWsBound, romUb_, static_cast<int32_t>(pc));
#endif
        LocalTensor<int32_t> f = bufI32(hatBase + 3U * halfLen_ + static_cast<uint32_t>(kPolysE_) * coeffN_ +
                                              aHatAivTileLength_ + hatPc,
                                          halfLen_);
        LocalTensor<int32_t> g =
            bufI32(hatBase + 3U * halfLen_ + static_cast<uint32_t>(kPolysE_) * coeffN_ + aHatAivTileLength_ + hatPc +
                       halfLen_,
                   halfLen_);
        LocalTensor<int32_t> t1m = bufI32(hatBase + 3U * halfLen_ + static_cast<uint32_t>(kPolysE_) * coeffN_ +
                                              aHatAivTileLength_ + hatPc + 2U * halfLen_,
                                          halfLen_);
        LocalTensor<int32_t> t2m = bufI32(hatBase + 3U * halfLen_ + static_cast<uint32_t>(kPolysE_) * coeffN_ +
                                              aHatAivTileLength_ + hatPc + 3U * halfLen_,
                                          halfLen_);

        for (uint16_t j = 0; j < static_cast<uint16_t>(tiling::kHatK); ++j) {
            for (uint32_t subOff = 0; subOff < coeffN_; subOff += halfLen_) {
                const int32_t gammaOff =
                    static_cast<int32_t>(subOff / halfLen_) * static_cast<int32_t>(halfLen_ / 2);
                const uint32_t sOff = twos1e::s_row(j) * coeffN_ + subOff;
                DataCopy(g, ub_ntt[sOff], halfLen_);
                ALG11_PIPE_MTE2();

                for (uint16_t p = pBegin_; p < pEnd_; ++p) {
                    const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * coeffN_;
                    const uint32_t aOff = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * aHatTileLength_ +
                                          static_cast<uint32_t>(j) * coeffN_ + subOff;
                    DataCopy(f, a_tile[aOff], halfLen_);
                    ALG11_PIPE_MTE2();
#if HAT_ALG11_VEC >= 1
                    hat_alg11::multiply_ntts_half_vec(row, f, g, basemulWsBound, romUb_, gammaSlice,
                                                      static_cast<int32_t>(pairCount_), gammaOff);
#else
                    multiply_ntts_half_scalar(row, f, g, static_cast<int32_t>(pairCount_), gammaOff);
#endif
                    ALG11_PIPE_ALL();

                    if (j == 0) {
                        DataCopy(partial[localP + subOff], row, halfLen_);
                        ALG11_PIPE_MTE2();
                    } else {
                        AscendC::Add(partial[localP + subOff], partial[localP + subOff], row,
                                     static_cast<int32_t>(halfLen_));
                        ALG11_PIPE_ALL();
                    }
                }
            }
        }

        for (uint16_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * coeffN_;
            const uint32_t eLp = twos1e::e_local_lp(p, pBegin_);

            for (uint32_t subOff = 0; subOff < coeffN_; subOff += halfLen_) {
                DataCopy(row, ub_ntt[eLp * coeffN_ + subOff], halfLen_);
                ALG11_PIPE_MTE2();
                AscendC::Add(acc, partial[localP + subOff], row, static_cast<int32_t>(halfLen_));
                ALG11_PIPE_ALL();
                MOD_Q_I32(acc, kHatQ, t1m, t2m, static_cast<int32_t>(halfLen_));
                ALG11_PIPE_ALL();
                DataCopy(line[subOff], acc, halfLen_);
                ALG11_PIPE_MTE2();
            }

            DataCopy(ub_that[localP], line, coeffN_);
            KYBER_PIPE_ALL();
        }
    }

    /** 行 19–20：t̂→ek_polyvec，ŝ[p]→sk_polyvec；每 AIV 负责 p∈[pBegin_,pEnd_) */
    __aicore__ inline void stageEncodeOut(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &ub_that)
    {
#if HAT_LINE18_FULLPOLY >= 1
        const uint32_t encByteBase = (ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ +
                                      3U * halfLen_ + limbTileLength_) *
                                     static_cast<uint32_t>(sizeof(int32_t));
#else
        const uint32_t encBase = ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ +
                                 limbTileLength_ + 3U * halfLen_ +
                                 static_cast<uint32_t>(tiling::kEPerAiv + 1) * coeffN_ + aHatAivTileLength_ +
#if HAT_ALG11_VEC >= 1
                                 hat_alg11_cfg::kHatPcMult * pairCount_ + 4U * halfLen_ +
                                     hat_alg11_cfg::kExtraInt32Slots;
#else
                                 10U * pairCount_ + 4U * halfLen_;
#endif
        const uint32_t encByteBase = encBase * static_cast<uint32_t>(sizeof(int32_t));
#endif
#if BYTE_ENCODE12_VEC >= 1
        const uint32_t encodeWsByteOff = encByteBase + 2U * byte_encode12::kAivShardBytes;
#if BYTE_ENCODE12_PREFETCH >= 1
        LocalTensor<int32_t> encode_ws = scratch_.GetWithOffset<int32_t>(
            byte_encode12::kPrefetchScratchInt32Slots, encodeWsByteOff);
#else
        LocalTensor<int32_t> encode_ws = scratch_.GetWithOffset<int32_t>(byte_encode12::kVecScratchInt32Slots,
                                                                         encodeWsByteOff);
#endif
#endif
        LocalTensor<uint8_t> ek_local =
            scratch_.GetWithOffset<uint8_t>(byte_encode12::kAivShardBytes, encByteBase);
        LocalTensor<uint8_t> sk_local = scratch_.GetWithOffset<uint8_t>(
            byte_encode12::kAivShardBytes, encByteBase + byte_encode12::kAivShardBytes);

        /* 对本核每个 p：先编 t̂→ek，再编 ŝ[p]→sk，写 GM polyvec 偏移 p*384 */
        for (uint16_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localIdx = static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_);
            const uint32_t byteLocal = localIdx * byte_encode12::kPolyBytes;
            const uint32_t byteGlobal = static_cast<uint32_t>(p) * byte_encode12::kPolyBytes;

            /* 行 19：ByteEncode₁₂(t̂[p]) → ek_polyvec */
            LocalTensor<int32_t> t_poly = ub_that[localIdx * coeffN_];
            LocalTensor<uint8_t> ek_poly = ek_local[byteLocal];
#if BYTE_ENCODE12_VEC >= 1
            byte_encode12::poly_byte_encode12_local(ek_poly, t_poly, coeffN_, encode_ws);
#else
            byte_encode12::poly_byte_encode12_scalar(ek_poly, t_poly, coeffN_);
#endif
            KYBER_PIPE_ALL();
            DataCopy(gm_ek_[byteGlobal], ek_poly, byte_encode12::kPolyBytes);
            KYBER_PIPE_ALL();

            /* 行 20：ByteEncode₁₂(ŝ[p]) → sk_polyvec（ŝ 行号即 p，poly-batch） */
            LocalTensor<int32_t> s_poly = ub_ntt[twos1e::s_row(p) * coeffN_];
            LocalTensor<uint8_t> sk_poly = sk_local[byteLocal];
#if BYTE_ENCODE12_VEC >= 1
            byte_encode12::poly_byte_encode12_local(sk_poly, s_poly, coeffN_, encode_ws);
#else
            byte_encode12::poly_byte_encode12_scalar(sk_poly, s_poly, coeffN_);
#endif
            KYBER_PIPE_ALL();
            DataCopy(gm_sk_[byteGlobal], sk_poly, byte_encode12::kPolyBytes);
            KYBER_PIPE_ALL();
        }
    }

    /** 对拍 dump：S3 后 ŝ/ê → gm_dst_dump（计算路径内不写 GM） */
    __aicore__ inline void dumpNttDebug(LocalTensor<int32_t> &ub_ntt)
    {
        const uint32_t sDstOff =
            (subCoreIdx_ == 0) ? static_cast<uint32_t>(tiling::dstSOffAiv0) : static_cast<uint32_t>(tiling::dstSOffAiv1);
        const uint32_t eDstOff =
            (subCoreIdx_ == 0) ? static_cast<uint32_t>(tiling::dstEOffAiv0) : static_cast<uint32_t>(tiling::dstEOffAiv1);
        DataCopy(gm_dst_dump_[sDstOff * coeffN_], ub_ntt, static_cast<uint32_t>(kPolysS_) * coeffN_);
        KYBER_PIPE_ALL();
        DataCopy(gm_dst_dump_[eDstOff * coeffN_], ub_ntt[static_cast<uint32_t>(kPolysS_) * coeffN_],
                 static_cast<uint32_t>(kPolysE_) * coeffN_);
        KYBER_PIPE_ALL();
    }

    /** 对拍 dump：行 18 后 t̂ → gm_t_dump */
    __aicore__ inline void dumpThatDebug(LocalTensor<int32_t> &ub_that)
    {
        for (uint16_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * coeffN_;
            DataCopy(gm_t_dump_[static_cast<uint32_t>(p) * coeffN_], ub_that[localP], coeffN_);
            KYBER_PIPE_ALL();
        }
    }

    int32_t subCoreIdx_;
    uint32_t coeffN_;
    uint32_t halfLen_;
    uint32_t pairCount_;
    uint16_t kPolysS_;
    uint16_t kPolysE_;
    uint16_t kUbPolys_;
    uint32_t limbTileLength_;
    uint32_t sBatchPlaneLength_;
    uint32_t eBatchPlaneLength_;
    uint32_t ubNttLength_;
    uint32_t aHatTileLength_;
    uint32_t aHatAivTileLength_;
    uint32_t thatTileLength_;
    uint16_t pBegin_;
    uint16_t pEnd_;
    uint32_t sSlotBase_;
    uint32_t eSlotBase_;
#if HAT_ALG11_VEC >= 1
    uint32_t alg11Base_;
    alg11_vec::RomUbLuts romUb_;
#endif

    AscendC::TPipe pipe_;
    AscendC::TBuf<AscendC::TPosition::VECCALC> ubNttBuf_;
    AscendC::TBuf<AscendC::TPosition::VECCALC> ubThatBuf_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_ub_ntt_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_ub_that_;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratch_;
#if HAT_LINE18_FULLPOLY >= 1
    AscendC::TBuf<AscendC::TPosition::VECCALC> dotScratchBuf_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> wsQue_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQueueF_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQueueG_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> gammaLutQue_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> gatherEvenQue_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> gatherOddQue_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> interleaveReorderQue_;
    alg11_vec::RomUbLuts dotRomUb_;
#endif
    AscendC::GlobalTensor<int32_t> gm_planar_, gm_a_, gm_dst_dump_, gm_t_dump_;
    AscendC::GlobalTensor<uint8_t> gm_ek_, gm_sk_;
};

#endif
