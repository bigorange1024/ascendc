#ifndef NTTS_2S1E_POST_NTT_UB_HPP
#define NTTS_2S1E_POST_NTT_UB_HPP

#include "aiv_func.hpp"
#include "basic.hpp"
#include "byte_encode12_config.hpp"
#include "byte_encode12_pair.hpp"
#include "hat_alg11_basemul.hpp"
#include "hat_line18_2s1e.hpp"
#include "hat_vec.hpp"
#include "integration_config.hpp"
#include "kernel_operator.h"
#include "mod_variants.hpp"
#include "ntt_vec.hpp"
#include "stage3_config.hpp"
#include "tiling.h"

using AscendC::DataCopy;

/**
 * Aiv2s1eUbPipeline — S3 → 行 18 → 行 19–20 单 TPipe UB 融合。
 *
 * 数据流（每 AIV 按 slot 分片）：
 *   1. DataCopy 平面 mat_c 8 行 → merge + stage31_mod → dst 局部
 *   2. 行 18 basemul：HAT_ALG11_VEC=1 → Alg11 B2+MEM_OPS=1；否则标量
 *   3. F203_MOD_BARRETT_VEC：ê 加 + mod（golden 仍 C 标量）
 *   4. poly_byte_encode12_local → ek/sk
 *
 * 约束：无 SHAT_PEER；ŝ/ê/t̂ 尽量驻留 UB。NTT S1–S3 禁止 Gather；ByteEncode 等 post-NTT 不在此限。
 */
class Aiv2s1eUbPipeline {
public:
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

    __aicore__ inline void Init(GM_ADDR matPlanar, GM_ADDR a_hat, GM_ADDR ek_out, GM_ADDR sk_out, GM_ADDR dst_dump,
                                GM_ADDR t_dump)
    {
        gm_planar_.SetGlobalBuffer((__gm__ int32_t *)matPlanar);
        gm_a_.SetGlobalBuffer((__gm__ int32_t *)a_hat);
        gm_ek_.SetGlobalBuffer((__gm__ uint8_t *)ek_out);
        gm_sk_.SetGlobalBuffer((__gm__ uint8_t *)sk_out);
        gm_dst_dump_.SetGlobalBuffer((__gm__ int32_t *)dst_dump);
        gm_t_dump_.SetGlobalBuffer((__gm__ int32_t *)t_dump);

        pipe_.InitBuffer(que_ub_ntt_, 1, ubNttLength_ * sizeof(int32_t));
        pipe_.InitBuffer(que_ub_that_, 1, thatTileLength_ * sizeof(int32_t));

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
        pipe_.InitBuffer(scratch_, (ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ +
                                    limbTileLength_ + halfLen_ + coeffN_ + aHatAivTileLength_ + hatPcMult * pc +
                                    4U * hl) *
                                       i32 +
                                   2U * byte_encode12::kAivShardBytes * u8
#if BYTE_ENCODE12_VEC >= 1
                                   + byte_encode12::kVecScratchBytes
#endif
#if HAT_ALG11_VEC >= 1
                                   + hat_alg11_cfg::kExtraInt32Slots * i32
#endif
#if F203_MOD_VARIANT == 2
                                   + 6U * hl * sizeof(float)
#endif
        );
#if HAT_ALG11_VEC >= 1
        alg11Base_ = ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ + limbTileLength_ +
                     3U * halfLen_ + 2U * halfLen_ + coeffN_ + aHatAivTileLength_ + hatPcMult * pc + 4U * hl;
        LocalTensor<int32_t> romBase = bufI32(alg11Base_, hat_alg11_cfg::kRomInt32Slots);
        hat_alg11::init_rom_luts(romBase, romUb_, static_cast<int32_t>(pc));
        KYBER_PIPE_ALL();
#endif
    }

    __aicore__ inline void Process(bool runS3Stage, bool runHat, bool runEncode, bool loadNttPreset,
                                   bool loadThatPreset)
    {
        LocalTensor<int32_t> ub_ntt = que_ub_ntt_.AllocTensor<int32_t>();
        LocalTensor<int32_t> ub_that = que_ub_that_.AllocTensor<int32_t>();

        if (loadNttPreset) {
            loadNttPresetInto(ub_ntt);
        } else if (runS3Stage) {
            stageS3Into(ub_ntt);
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
            stageEncodeOut(ub_ntt, ub_that);
        }

        if (runS3Stage && !loadNttPreset) {
            dumpNttDebug(ub_ntt);
        }
        if (runHat && !loadThatPreset) {
            dumpThatDebug(ub_that);
        }

        que_ub_that_.FreeTensor(ub_that);
        que_ub_ntt_.FreeTensor(ub_ntt);
        KYBER_PIPE_ALL();
    }

private:
    __aicore__ inline LocalTensor<int32_t> bufI32(uint32_t offElems, uint32_t nElems)
    {
        return scratch_.GetWithOffset<int32_t>(nElems, offElems * static_cast<uint32_t>(sizeof(int32_t)));
    }

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

    __aicore__ inline void stageS3Into(LocalTensor<int32_t> &ub_ntt)
    {
        mergeBankInto(ub_ntt, 0U, sSlotBase_, kPolysS_, sBatchPlaneLength_);
        mergeBankInto(ub_ntt, static_cast<uint32_t>(kPolysS_), eSlotBase_, kPolysE_, eBatchPlaneLength_);
    }

#if F203_STAGE3_MOD == 2
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

    __aicore__ inline void mergePolyHalf(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &plane,
                                         LocalTensor<int32_t> &half_out, LocalTensor<int32_t> &t1,
                                         LocalTensor<float> &fRaw, LocalTensor<float> &fTmp, LocalTensor<float> &fQuot,
                                         uint32_t ubLp, uint16_t lp, uint32_t halfIdx)
#else
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

    __aicore__ inline void mergePolyHalf(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &plane,
                                         LocalTensor<int32_t> &half_out, LocalTensor<int32_t> &t1,
                                         LocalTensor<int32_t> &t2, uint32_t ubLp, uint16_t lp, uint32_t halfIdx)
#endif
    {
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

    __aicore__ inline void stageHatInto(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &ub_that)
    {
        const uint32_t pc = pairCount_;
        const uint32_t hatBase =
            ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ + limbTileLength_ + 3U * halfLen_;

        LocalTensor<int32_t> acc = bufI32(hatBase, halfLen_);
        LocalTensor<int32_t> row = bufI32(hatBase + halfLen_, halfLen_);
        LocalTensor<int32_t> line = bufI32(hatBase + 2U * halfLen_, coeffN_);
        LocalTensor<int32_t> a_tile = bufI32(hatBase + 2U * halfLen_ + coeffN_, aHatAivTileLength_);
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
        LocalTensor<int32_t> f = bufI32(hatBase + 2U * halfLen_ + coeffN_ + aHatAivTileLength_ + hatPc, halfLen_);
        LocalTensor<int32_t> g =
            bufI32(hatBase + 2U * halfLen_ + coeffN_ + aHatAivTileLength_ + hatPc + halfLen_, halfLen_);
        LocalTensor<int32_t> t1m =
            bufI32(hatBase + 2U * halfLen_ + coeffN_ + aHatAivTileLength_ + hatPc + 2U * halfLen_, halfLen_);
        LocalTensor<int32_t> t2m =
            bufI32(hatBase + 2U * halfLen_ + coeffN_ + aHatAivTileLength_ + hatPc + 3U * halfLen_, halfLen_);
#if F203_MOD_VARIANT == 2
        const uint32_t fOff = hatBase + 2U * halfLen_ + coeffN_ + aHatAivTileLength_ + hatPc + 4U * halfLen_;
        const uint32_t fStride = halfLen_ * static_cast<uint32_t>(sizeof(float));
        LocalTensor<float> fRaw = scratch_.GetWithOffset<float>(halfLen_, fOff * sizeof(int32_t));
        LocalTensor<float> fTmp = scratch_.GetWithOffset<float>(halfLen_, fOff * sizeof(int32_t) + fStride);
        LocalTensor<float> fQuot = scratch_.GetWithOffset<float>(halfLen_, fOff * sizeof(int32_t) + 2U * fStride);
#endif

        for (uint16_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t aPolyOff =
                (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * aHatTileLength_;
            const uint32_t eLp = twos1e::e_local_lp(p, pBegin_);
            DataCopy(line, ub_ntt[eLp * coeffN_], coeffN_);
            KYBER_PIPE_ALL();

            for (uint32_t subOff = 0; subOff < coeffN_; subOff += halfLen_) {
                const int32_t gammaOff =
                    static_cast<int32_t>(subOff / halfLen_) * static_cast<int32_t>(halfLen_ / 2);
                AscendC::Duplicate(acc, static_cast<int32_t>(0), static_cast<int32_t>(halfLen_));
                KYBER_PIPE_ALL();

                for (uint16_t j = 0; j < static_cast<uint16_t>(tiling::kHatK); ++j) {
                    const uint32_t aOff = aPolyOff + static_cast<uint32_t>(j) * coeffN_ + subOff;
                    const uint32_t sOff = twos1e::s_row(j) * coeffN_ + subOff;
                    DataCopy(f, a_tile[aOff], halfLen_);
                    DataCopy(g, ub_ntt[sOff], halfLen_);
                    KYBER_PIPE_ALL();
#if HAT_ALG11_VEC >= 1
                    hat_alg11::multiply_ntts_half_vec(row, f, g, basemulWsBound, romUb_, gammaSlice,
                                                      static_cast<int32_t>(pairCount_), gammaOff);
#else
                    multiply_ntts_half_scalar(row, f, g, static_cast<int32_t>(pairCount_), gammaOff);
#endif
                    AscendC::Add(acc, acc, row, static_cast<int32_t>(halfLen_));
                    KYBER_PIPE_ALL();
                }

                DataCopy(row, line[subOff], halfLen_);
                KYBER_PIPE_ALL();
                AscendC::Add(acc, acc, row, static_cast<int32_t>(halfLen_)); // 向量加 ê[p]
                KYBER_PIPE_ALL();
#if F203_MOD_VARIANT == 2
                MOD_Q_CAST(acc, kHatQ, t1m, fRaw, fTmp, fQuot, static_cast<int32_t>(halfLen_));
#else
                MOD_Q_I32(acc, kHatQ, t1m, t2m, static_cast<int32_t>(halfLen_)); // 向量 final mod
#endif
                KYBER_PIPE_ALL();
                DataCopy(line[subOff], acc, halfLen_);
                KYBER_PIPE_ALL();
            }

            const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * coeffN_;
            DataCopy(ub_that[localP], line, coeffN_);
            KYBER_PIPE_ALL();
        }
    }

    __aicore__ inline void stageEncodeOut(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &ub_that)
    {
        const uint32_t encBase = ubNttLength_ + thatTileLength_ + sBatchPlaneLength_ + eBatchPlaneLength_ +
                                 limbTileLength_ + 3U * halfLen_ + 2U * halfLen_ + coeffN_ + aHatAivTileLength_ +
#if HAT_ALG11_VEC >= 1
                                 hat_alg11_cfg::kHatPcMult * pairCount_ + 4U * halfLen_ +
                                     hat_alg11_cfg::kExtraInt32Slots;
#else
                                 10U * pairCount_ + 4U * halfLen_;
#endif
        const uint32_t encByteBase = encBase * static_cast<uint32_t>(sizeof(int32_t));
#if BYTE_ENCODE12_VEC >= 1
        const uint32_t encodeWsByteOff = encByteBase + 2U * byte_encode12::kAivShardBytes;
        LocalTensor<int32_t> encode_ws = scratch_.GetWithOffset<int32_t>(byte_encode12::kVecScratchInt32Slots,
                                                                         encodeWsByteOff);
#endif
        LocalTensor<uint8_t> ek_local =
            scratch_.GetWithOffset<uint8_t>(byte_encode12::kAivShardBytes, encByteBase);
        LocalTensor<uint8_t> sk_local = scratch_.GetWithOffset<uint8_t>(
            byte_encode12::kAivShardBytes, encByteBase + byte_encode12::kAivShardBytes);

        for (uint16_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localIdx = static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_);
            const uint32_t byteLocal = localIdx * byte_encode12::kPolyBytes;
            const uint32_t byteGlobal = static_cast<uint32_t>(p) * byte_encode12::kPolyBytes;

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
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_ub_ntt_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_ub_that_;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratch_;
    AscendC::GlobalTensor<int32_t> gm_planar_, gm_a_, gm_dst_dump_, gm_t_dump_;
    AscendC::GlobalTensor<uint8_t> gm_ek_, gm_sk_;
};

#endif
