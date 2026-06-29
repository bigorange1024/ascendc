#ifndef ALG13_POST_NTT_UB_HPP
#define ALG13_POST_NTT_UB_HPP

#include "aiv_func.hpp"
#include "basic.hpp"
#include "byte_encode12_pair.hpp"
#include "hat_line18_pair.hpp"
#include "hat_vec.hpp"
#include "kernel_operator.h"
#include "mod_variants.hpp"
#include "ntt_vec.hpp"
#include "tiling.h"

using AscendC::DataCopy;

/** S3 → 行 18 → 行 19–20：单 TPipe，ŝ/ê/t̂ 全程 UB；对端 ŝ 仅 ws 握手；ek/sk 为首次算法 GM 输出。 */
class AivAlg13UbPipeline {
public:
    __aicore__ inline AivAlg13UbPipeline(int32_t subCoreIdx, uint32_t coeffN, bool hatMaster = false)
        : subCoreIdx(subCoreIdx), coeffN(coeffN), halfLen(coeffN / 2), pairCount(halfLen / 2),
          hatMasterMode(hatMaster),
          kPolysLocal(static_cast<uint16_t>(tiling::kPolysPerAiv)),
          polyStart(static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kPolysPerAiv)),
          limbTileLength(4 * (coeffN / 2)),
          batchHalfPlaneLength(static_cast<uint32_t>(tiling::kPolysPerAiv) * 4 * (coeffN / 2)),
          outTileLength(static_cast<uint32_t>(tiling::kPolysPerAiv) * coeffN),
          aHatTileLength(static_cast<uint32_t>(tiling::kHatK) * coeffN),
          aHatAivTileLength(hatMaster ? static_cast<uint32_t>(tiling::kHatK) * static_cast<uint32_t>(tiling::kHatK) *
                                            coeffN
                                      : static_cast<uint32_t>(sepair::p_end(subCoreIdx) - sepair::p_begin(subCoreIdx)) *
                                            static_cast<uint32_t>(tiling::kHatK) * coeffN),
          thatTileLength(hatMaster ? static_cast<uint32_t>(tiling::kHatK) * coeffN
                                   : static_cast<uint32_t>(sepair::p_end(subCoreIdx) - sepair::p_begin(subCoreIdx)) *
                                         coeffN),
          encodeScratchBytes(hatMaster ? byte_encode12::kPolyBytes * static_cast<uint32_t>(tiling::kHatK)
                                       : byte_encode12::kAivShardBytes),
          pBegin(hatMaster ? static_cast<uint16_t>(0) : sepair::p_begin(subCoreIdx)),
          pEnd(hatMaster ? static_cast<uint16_t>(tiling::kHatK) : sepair::p_end(subCoreIdx))
    {
    }

    __aicore__ inline void Init(GM_ADDR matPlanar, GM_ADDR peerShat, GM_ADDR a_hat, GM_ADDR ek_out, GM_ADDR sk_out,
                                GM_ADDR dst_dump, GM_ADDR t_dump)
    {
        gm_planar.SetGlobalBuffer((__gm__ int32_t *)matPlanar);
        gm_peer.SetGlobalBuffer((__gm__ int32_t *)peerShat);
        gm_a.SetGlobalBuffer((__gm__ int32_t *)a_hat);
        gm_ek.SetGlobalBuffer((__gm__ uint8_t *)ek_out);
        gm_sk.SetGlobalBuffer((__gm__ uint8_t *)sk_out);
        gm_dst_dump.SetGlobalBuffer((__gm__ int32_t *)dst_dump);
        gm_t_dump.SetGlobalBuffer((__gm__ int32_t *)t_dump);

        pipe.InitBuffer(que_ub_ntt, 1, outTileLength * sizeof(int32_t));
        pipe.InitBuffer(que_ub_that, 1, thatTileLength * sizeof(int32_t));

        const uint32_t pc = pairCount;
        const uint32_t hl = halfLen;
        const uint32_t i32 = sizeof(int32_t);
        const uint32_t u8 = sizeof(uint8_t);
        pipe.InitBuffer(scratch, (outTileLength + thatTileLength + batchHalfPlaneLength + limbTileLength + halfLen +
                                  coeffN + aHatAivTileLength + 10U * pc + 4U * hl +
                                  (hatMasterMode ? coeffN : 0U)) *
                                     i32 +
                                 2U * encodeScratchBytes * u8
#if F203_MOD_VARIANT == 2
                                 + 6U * hl * sizeof(float)
#endif
        );
    }

    __aicore__ inline void Process(bool runS3Stage, bool runHat, bool runEncode, bool loadNttPreset,
                                   bool loadThatPreset)
    {
        ub_ntt = que_ub_ntt.AllocTensor<int32_t>();
        ub_that = que_ub_that.AllocTensor<int32_t>();

        if (loadNttPreset) {
            const uint32_t dstBase = static_cast<uint32_t>(polyStart) * coeffN;
            DataCopy(ub_ntt, gm_dst_dump[dstBase], outTileLength);
            KYBER_PIPE_ALL();
        } else if (runS3Stage) {
            stageS3Into(ub_ntt);
        }

        if (loadThatPreset) {
            for (uint16_t p = pBegin; p < pEnd; ++p) {
                const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin)) * coeffN;
                DataCopy(ub_that[localP], gm_t_dump[static_cast<uint32_t>(p) * coeffN], coeffN);
                KYBER_PIPE_ALL();
            }
        } else if (runHat) {
            stageHatInto(ub_ntt, ub_that, loadNttPreset && !runS3Stage);
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

        que_ub_that.FreeTensor(ub_that);
        que_ub_ntt.FreeTensor(ub_ntt);
        KYBER_PIPE_ALL();
    }

    /** mixPass=0 非 master：S3 + 发布本地 ŝ‖ê tile 到 ws。 */
    __aicore__ inline void ProcessPublishOnly(bool runS3Stage)
    {
        ub_ntt = que_ub_ntt.AllocTensor<int32_t>();
        ub_that = que_ub_that.AllocTensor<int32_t>();
        if (runS3Stage) {
            stageS3Into(ub_ntt);
            publishSeTile(ub_ntt);
            dumpNttDebug(ub_ntt);
        }
        que_ub_that.FreeTensor(ub_that);
        que_ub_ntt.FreeTensor(ub_ntt);
        KYBER_PIPE_ALL();
    }

    /** mixPass=0 master AIV：在双方 ws tile 就绪后行 18 + encode。 */
    __aicore__ inline void ProcessHatEncodeMaster(bool runEncode, bool runS3Stage)
    {
        ub_ntt = que_ub_ntt.AllocTensor<int32_t>();
        ub_that = que_ub_that.AllocTensor<int32_t>();
        if (runS3Stage) {
            stageS3Into(ub_ntt);
            publishSeTile(ub_ntt);
            dumpNttDebug(ub_ntt);
        }
        stageHatInto(ub_ntt, ub_that, false);
        if (runEncode) {
            stageEncodeOut(ub_ntt, ub_that);
        }
        dumpThatDebug(ub_that);
        que_ub_that.FreeTensor(ub_that);
        que_ub_ntt.FreeTensor(ub_ntt);
        KYBER_PIPE_ALL();
    }

    __aicore__ inline LocalTensor<int32_t> bufI32(uint32_t offElems, uint32_t nElems)
    {
        return scratch.GetWithOffset<int32_t>(nElems, offElems * static_cast<uint32_t>(sizeof(int32_t)));
    }

    __aicore__ inline uint32_t localLpForSRow(uint16_t j) const
    {
        return sepair::s_row(j) - static_cast<uint32_t>(polyStart);
    }

    __aicore__ inline uint32_t peerTileOffset(uint32_t globalRow) const
    {
        const uint32_t aiv = globalRow / static_cast<uint32_t>(tiling::kPolysPerAiv);
        const uint32_t lp = globalRow % static_cast<uint32_t>(tiling::kPolysPerAiv);
        return static_cast<uint32_t>(aiv) * outTileLength + lp * coeffN;
    }

    __aicore__ inline void readSeRow(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &ub_ntt, uint32_t globalRow,
                                     uint32_t len)
    {
        const uint32_t aiv = globalRow / static_cast<uint32_t>(tiling::kPolysPerAiv);
        const uint32_t lp = globalRow % static_cast<uint32_t>(tiling::kPolysPerAiv);
        if (!hatMasterMode && aiv == static_cast<uint32_t>(subCoreIdx)) {
            DataCopy(dst, ub_ntt[lp * coeffN], len);
        } else {
            DataCopy(dst, gm_peer[peerTileOffset(globalRow)], len);
        }
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void stageS3Into(LocalTensor<int32_t> &ub_ntt)
    {
        LocalTensor<int32_t> plane = bufI32(outTileLength + thatTileLength, batchHalfPlaneLength);
        LocalTensor<int32_t> half_out = bufI32(outTileLength + thatTileLength + batchHalfPlaneLength, halfLen);
        LocalTensor<int32_t> t1 = bufI32(outTileLength + thatTileLength + batchHalfPlaneLength + halfLen, halfLen);
#if F203_MOD_VARIANT == 0
        LocalTensor<int32_t> t2 = bufI32(outTileLength + thatTileLength + batchHalfPlaneLength + 2U * halfLen, halfLen);
#elif F203_MOD_VARIANT == 2
        const uint32_t fBase = outTileLength + thatTileLength + batchHalfPlaneLength + 2U * halfLen;
        const uint32_t fStride = halfLen * static_cast<uint32_t>(sizeof(float));
        LocalTensor<float> fRaw = scratch.GetWithOffset<float>(halfLen, fBase * sizeof(int32_t));
        LocalTensor<float> fTmp = scratch.GetWithOffset<float>(halfLen, fBase * sizeof(int32_t) + fStride);
        LocalTensor<float> fQuot = scratch.GetWithOffset<float>(halfLen, fBase * sizeof(int32_t) + 2U * fStride);
#else
        LocalTensor<int32_t> t2 = t1;
#endif

        const uint32_t loPlaneBase = planar::mat_row(static_cast<uint32_t>(polyStart), 0U, 0U) * halfLen;
        const uint32_t hiPlaneBase = planar::mat_row(static_cast<uint32_t>(polyStart), 0U, 1U) * halfLen;
        DataCopy(plane, gm_planar[loPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();

        for (uint16_t lp = 0; lp < kPolysLocal; lp++) {
            mergePolyHalf(ub_ntt, plane, half_out, t1,
#if F203_MOD_VARIANT == 2
                          fRaw, fTmp, fQuot,
#else
                          t2,
#endif
                          lp, 0U);
        }

        DataCopy(plane, gm_planar[hiPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();
        for (uint16_t lp = 0; lp < kPolysLocal; lp++) {
            mergePolyHalf(ub_ntt, plane, half_out, t1,
#if F203_MOD_VARIANT == 2
                          fRaw, fTmp, fQuot,
#else
                          t2,
#endif
                          lp, 1U);
        }
        KYBER_PIPE_ALL();
    }

#if F203_MOD_VARIANT == 2
    __aicore__ inline void mergePolyHalf(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &plane,
                                         LocalTensor<int32_t> &half_out, LocalTensor<int32_t> &t1,
                                         LocalTensor<float> &fRaw, LocalTensor<float> &fTmp, LocalTensor<float> &fQuot,
                                         uint16_t lp, uint32_t halfIdx)
#else
    __aicore__ inline void mergePolyHalf(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &plane,
                                         LocalTensor<int32_t> &half_out, LocalTensor<int32_t> &t1,
                                         LocalTensor<int32_t> &t2, uint16_t lp, uint32_t halfIdx)
#endif
    {
        const uint32_t limbBase = outTileLength + thatTileLength + batchHalfPlaneLength + 3U * halfLen;
        LocalTensor<int32_t> limbs = bufI32(limbBase, limbTileLength);
        const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN;
        const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength;
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
        if (halfIdx == 0U) {
            DataCopy(ub_ntt[outBase], half_out, halfLen);
        } else {
            DataCopy(ub_ntt[outBase + halfLen], half_out, halfLen);
        }
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void publishSeTile(LocalTensor<int32_t> &ub_ntt)
    {
        const uint32_t slotBase = static_cast<uint32_t>(subCoreIdx) * outTileLength;
        DataCopy(gm_peer[slotBase], ub_ntt, outTileLength);
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void stageHatInto(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &ub_that, bool peerFromDstPreset)
    {
        const uint32_t pc = pairCount;
        const uint32_t pcBytes = pc * sizeof(int32_t);
        const uint32_t hlBytes = halfLen * sizeof(int32_t);
        const uint32_t hatBase = outTileLength + thatTileLength + batchHalfPlaneLength + limbTileLength + 3U * halfLen;

        LocalTensor<int32_t> acc = bufI32(hatBase, halfLen);
        LocalTensor<int32_t> row = bufI32(hatBase + halfLen, halfLen);
        LocalTensor<int32_t> line = bufI32(hatBase + 2U * halfLen, coeffN);
        LocalTensor<int32_t> a_tile = bufI32(hatBase + 2U * halfLen + coeffN, aHatAivTileLength);
        const uint32_t aAivBase = static_cast<uint32_t>(pBegin) * static_cast<uint32_t>(tiling::kHatK) * coeffN;
        DataCopy(a_tile, gm_a[aAivBase], aHatAivTileLength);
        KYBER_PIPE_ALL();

        LocalTensor<int32_t> f = bufI32(hatBase + 2U * halfLen + coeffN + aHatAivTileLength + 10U * pc, halfLen);
        LocalTensor<int32_t> g = bufI32(hatBase + 2U * halfLen + coeffN + aHatAivTileLength + 10U * pc + halfLen, halfLen);
        LocalTensor<int32_t> t1m =
            bufI32(hatBase + 2U * halfLen + coeffN + aHatAivTileLength + 10U * pc + 2U * halfLen, halfLen);
        LocalTensor<int32_t> t2m =
            bufI32(hatBase + 2U * halfLen + coeffN + aHatAivTileLength + 10U * pc + 3U * halfLen, halfLen);
#if F203_MOD_VARIANT == 2
        const uint32_t fOff = hatBase + 2U * halfLen + coeffN + aHatAivTileLength + 10U * pc + 4U * halfLen;
        const uint32_t fStride = halfLen * static_cast<uint32_t>(sizeof(float));
        LocalTensor<float> fRaw = scratch.GetWithOffset<float>(halfLen, fOff * sizeof(int32_t));
        LocalTensor<float> fTmp = scratch.GetWithOffset<float>(halfLen, fOff * sizeof(int32_t) + fStride);
        LocalTensor<float> fQuot = scratch.GetWithOffset<float>(halfLen, fOff * sizeof(int32_t) + 2U * fStride);
#endif

        for (uint16_t p = pBegin; p < pEnd; ++p) {
            const uint32_t aPolyOff =
                (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin)) * aHatTileLength;
            if (hatMasterMode) {
                readSeRow(line, ub_ntt, sepair::e_row(p), coeffN);
            } else if (peerFromDstPreset) {
                const uint32_t eOff = sepair::e_row(p) * coeffN;
                DataCopy(line, gm_dst_dump[eOff], coeffN);
                KYBER_PIPE_ALL();
            } else {
                const uint32_t eLp = sepair::e_row(p) - static_cast<uint32_t>(polyStart);
                DataCopy(line, ub_ntt[eLp * coeffN], coeffN);
                KYBER_PIPE_ALL();
            }

            for (uint32_t subOff = 0; subOff < coeffN; subOff += halfLen) {
                const int32_t gammaOff =
                    static_cast<int32_t>(subOff / halfLen) * static_cast<int32_t>(halfLen / 2);
                AscendC::Duplicate(acc, static_cast<int32_t>(0), static_cast<int32_t>(halfLen));
                KYBER_PIPE_ALL();

                for (uint16_t j = 0; j < tiling::kHatK; ++j) {
                    const uint32_t aOff = aPolyOff + static_cast<uint32_t>(j) * coeffN + subOff;
                    DataCopy(f, a_tile[aOff], halfLen);
                    if (hatMasterMode) {
                        DataCopy(g, gm_peer[peerTileOffset(sepair::s_row(j)) + subOff], halfLen);
                    } else if (sepair::j_local(subCoreIdx, j)) {
                        DataCopy(g, ub_ntt[localLpForSRow(j) * coeffN + subOff], halfLen);
                    } else if (peerFromDstPreset) {
                        const uint32_t sOff = sepair::s_row(j) * coeffN + subOff;
                        DataCopy(g, gm_dst_dump[sOff], halfLen);
                    } else {
                        const int32_t peerCore = 1 - subCoreIdx;
                        const uint32_t peerSlot = static_cast<uint32_t>(peerCore) * outTileLength;
                        const uint32_t peerRow =
                            (sepair::s_row(j) % static_cast<uint32_t>(tiling::kPolysPerAiv)) * coeffN;
                        DataCopy(g, gm_peer[peerSlot + peerRow + subOff], halfLen);
                    }
                    KYBER_PIPE_ALL();
                    multiply_ntts_half_scalar(row, f, g, static_cast<int32_t>(pairCount), gammaOff);
                    AscendC::Add(acc, acc, row, static_cast<int32_t>(halfLen));
                    KYBER_PIPE_ALL();
                }

                DataCopy(row, line[subOff], halfLen);
                KYBER_PIPE_ALL();
                AscendC::Add(acc, acc, row, static_cast<int32_t>(halfLen));
                KYBER_PIPE_ALL();
#if F203_MOD_VARIANT == 2
                MOD_Q_CAST(acc, kHatQ, t1m, fRaw, fTmp, fQuot, static_cast<int32_t>(halfLen));
#else
                MOD_Q_I32(acc, kHatQ, t1m, t2m, static_cast<int32_t>(halfLen));
#endif
                KYBER_PIPE_ALL();
                DataCopy(line[subOff], acc, halfLen);
                KYBER_PIPE_ALL();
            }

            const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin)) * coeffN;
            DataCopy(ub_that[localP], line, coeffN);
            KYBER_PIPE_ALL();
        }
        (void)pcBytes;
        (void)hlBytes;
    }

    __aicore__ inline void stageEncodeOut(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &ub_that)
    {
        const uint32_t encBase = outTileLength + thatTileLength + batchHalfPlaneLength + limbTileLength + 3U * halfLen +
                                2U * halfLen + coeffN + aHatAivTileLength + 10U * pairCount + 4U * halfLen;
        LocalTensor<uint8_t> ek_local =
            scratch.GetWithOffset<uint8_t>(encodeScratchBytes, encBase * sizeof(int32_t));
        LocalTensor<uint8_t> sk_local = scratch.GetWithOffset<uint8_t>(
            encodeScratchBytes, (encBase * sizeof(int32_t)) + encodeScratchBytes);
        const uint32_t encPolyScratch = hatMasterMode ? coeffN : 0U;
        LocalTensor<int32_t> s_scratch =
            encPolyScratch > 0U ? bufI32(encBase + (2U * encodeScratchBytes) / sizeof(int32_t), coeffN)
                                : ub_ntt[0];

        for (uint16_t p = pBegin; p < pEnd; ++p) {
            const uint32_t localIdx = static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin);
            const uint32_t byteLocal = localIdx * byte_encode12::kPolyBytes;
            const uint32_t byteGlobal = static_cast<uint32_t>(p) * byte_encode12::kPolyBytes;

            LocalTensor<int32_t> t_poly = ub_that[localIdx * coeffN];
            LocalTensor<uint8_t> ek_poly = ek_local[byteLocal];
            byte_encode12::poly_byte_encode12_local(ek_poly, t_poly, coeffN);
            KYBER_PIPE_ALL();
            DataCopy(gm_ek[byteGlobal], ek_poly, byte_encode12::kPolyBytes);
            KYBER_PIPE_ALL();

            LocalTensor<int32_t> s_poly = hatMasterMode
                                              ? s_scratch
                                              : ub_ntt[localLpForSRow(static_cast<uint16_t>(p)) * coeffN];
            if (hatMasterMode) {
                readSeRow(s_poly, ub_ntt, sepair::s_row(static_cast<uint16_t>(p)), coeffN);
            }
            LocalTensor<uint8_t> sk_poly = sk_local[byteLocal];
            byte_encode12::poly_byte_encode12_local(sk_poly, s_poly, coeffN);
            KYBER_PIPE_ALL();
            DataCopy(gm_sk[byteGlobal], sk_poly, byte_encode12::kPolyBytes);
            KYBER_PIPE_ALL();
        }
    }

    __aicore__ inline void dumpNttDebug(LocalTensor<int32_t> &ub_ntt)
    {
        const uint32_t dstBase = static_cast<uint32_t>(polyStart) * coeffN;
        DataCopy(gm_dst_dump[dstBase], ub_ntt, outTileLength);
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void dumpThatDebug(LocalTensor<int32_t> &ub_that)
    {
        for (uint16_t p = pBegin; p < pEnd; ++p) {
            const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin)) * coeffN;
            DataCopy(gm_t_dump[static_cast<uint32_t>(p) * coeffN], ub_that[localP], coeffN);
            KYBER_PIPE_ALL();
        }
    }

    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint32_t halfLen;
    const uint32_t pairCount;
    const bool hatMasterMode;
    const uint16_t kPolysLocal;
    const uint32_t encodeScratchBytes;
    const uint16_t polyStart;
    const uint32_t limbTileLength;
    const uint32_t batchHalfPlaneLength;
    const uint32_t outTileLength;
    const uint32_t aHatTileLength;
    const uint32_t aHatAivTileLength;
    const uint32_t thatTileLength;
    const uint16_t pBegin;
    const uint16_t pEnd;

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_ub_ntt;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_ub_that;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratch;
    AscendC::GlobalTensor<int32_t> gm_planar, gm_peer, gm_a, gm_dst_dump, gm_t_dump;
    AscendC::GlobalTensor<uint8_t> gm_ek, gm_sk;
    LocalTensor<int32_t> ub_ntt;
    LocalTensor<int32_t> ub_that;
};

#endif
