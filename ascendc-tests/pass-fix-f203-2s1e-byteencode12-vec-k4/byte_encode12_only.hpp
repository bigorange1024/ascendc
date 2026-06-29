#ifndef BYTE_ENCODE12_ONLY_HPP
#define BYTE_ENCODE12_ONLY_HPP

#include "basic.hpp"
#include "byte_encode12_config.hpp"
#include "kyber_limb6.hpp"
#include "byte_encode12_pair.hpp"
#include "hat_line18_2s1e.hpp"
#include "kernel_operator.h"
#include "tiling.h"

using AscendC::DataCopy;

/** 2×AIV：preset ŝ‖ê + t̂ → ByteEncode₁₂(ek/sk)。 */
class AivByteEncode12Only {
public:
    __aicore__ inline AivByteEncode12Only(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN),
          kPolysS_(static_cast<uint16_t>(tiling::kS)),
          kPolysE_(static_cast<uint16_t>(tiling::kEPerAiv)),
          ubNttLength_(static_cast<uint32_t>(tiling::kS + tiling::kEPerAiv) * coeffN),
          thatTileLength_(static_cast<uint32_t>(tiling::kEPerAiv) * coeffN),
          pBegin_(twos1e::p_begin(subCoreIdx)), pEnd_(twos1e::p_end(subCoreIdx))
    {
    }

    __aicore__ inline void Init(GM_ADDR dst_gm, GM_ADDR t_hat_gm, GM_ADDR ek_out, GM_ADDR sk_out)
    {
        gm_dst_.SetGlobalBuffer((__gm__ int32_t *)dst_gm);
        gm_t_hat_.SetGlobalBuffer((__gm__ int32_t *)t_hat_gm);
        gm_ek_.SetGlobalBuffer((__gm__ uint8_t *)ek_out);
        gm_sk_.SetGlobalBuffer((__gm__ uint8_t *)sk_out);

        pipe_.InitBuffer(que_ub_ntt_, 1, ubNttLength_ * sizeof(int32_t));
        pipe_.InitBuffer(que_ub_that_, 1, thatTileLength_ * sizeof(int32_t));
        pipe_.InitBuffer(scratch_, 2U * byte_encode12::kAivShardBytes
#if BYTE_ENCODE12_VEC >= 1 && BYTE_ENCODE12_PREFETCH >= 1
                             + byte_encode12::kPrefetchScratchBytes
#elif BYTE_ENCODE12_VEC >= 1
                             + byte_encode12::kVecScratchBytes
#endif
        );
    }

    __aicore__ inline void Process()
    {
        LocalTensor<int32_t> ub_ntt = que_ub_ntt_.AllocTensor<int32_t>();
        LocalTensor<int32_t> ub_that = que_ub_that_.AllocTensor<int32_t>();

        loadNttPresetInto(ub_ntt);
        loadThatPresetInto(ub_that);
        stageEncodeOut(ub_ntt, ub_that);

        que_ub_that_.FreeTensor(ub_that);
        que_ub_ntt_.FreeTensor(ub_ntt);
        KYBER_PIPE_ALL();
    }

private:
    __aicore__ inline void loadNttPresetInto(LocalTensor<int32_t> &ub_ntt)
    {
        const uint32_t sDstOff =
            (subCoreIdx_ == 0) ? static_cast<uint32_t>(tiling::dstSOffAiv0) : static_cast<uint32_t>(tiling::dstSOffAiv1);
        const uint32_t eDstOff =
            (subCoreIdx_ == 0) ? static_cast<uint32_t>(tiling::dstEOffAiv0) : static_cast<uint32_t>(tiling::dstEOffAiv1);
        DataCopy(ub_ntt, gm_dst_[sDstOff * coeffN_], static_cast<uint32_t>(kPolysS_) * coeffN_);
        KYBER_PIPE_ALL();
        DataCopy(ub_ntt[static_cast<uint32_t>(kPolysS_) * coeffN_], gm_dst_[eDstOff * coeffN_],
                 static_cast<uint32_t>(kPolysE_) * coeffN_);
        KYBER_PIPE_ALL();
    }

    __aicore__ inline void loadThatPresetInto(LocalTensor<int32_t> &ub_that)
    {
        for (uint16_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * coeffN_;
            DataCopy(ub_that[localP], gm_t_hat_[static_cast<uint32_t>(p) * coeffN_], coeffN_);
            KYBER_PIPE_ALL();
        }
    }

    __aicore__ inline void stageEncodeOut(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &ub_that)
    {
#if BYTE_ENCODE12_VEC >= 1 && BYTE_ENCODE12_PREFETCH >= 1
        LocalTensor<int32_t> encode_ws = scratch_.GetWithOffset<int32_t>(
            byte_encode12::kPrefetchScratchInt32Slots, 2U * byte_encode12::kAivShardBytes);
#elif BYTE_ENCODE12_VEC >= 1
        LocalTensor<int32_t> encode_ws = scratch_.GetWithOffset<int32_t>(
            byte_encode12::kVecScratchInt32Slots, 2U * byte_encode12::kAivShardBytes);
#endif
        LocalTensor<uint8_t> ek_local = scratch_.GetWithOffset<uint8_t>(byte_encode12::kAivShardBytes, 0U);
        LocalTensor<uint8_t> sk_local =
            scratch_.GetWithOffset<uint8_t>(byte_encode12::kAivShardBytes, byte_encode12::kAivShardBytes);

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

    int32_t subCoreIdx_;
    uint32_t coeffN_;
    uint16_t kPolysS_;
    uint16_t kPolysE_;
    uint32_t ubNttLength_;
    uint32_t thatTileLength_;
    uint16_t pBegin_;
    uint16_t pEnd_;

    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_ub_ntt_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_ub_that_;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratch_;
    AscendC::GlobalTensor<int32_t> gm_dst_, gm_t_hat_;
    AscendC::GlobalTensor<uint8_t> gm_ek_, gm_sk_;
};

#endif
