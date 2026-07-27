#ifndef BYTE_ENCODE12_ONLY_HPP
#define BYTE_ENCODE12_ONLY_HPP

/**
 * @file byte_encode12_only.hpp
 * @brief 2×AIV ByteEncode₁₂-only 流水：preset ŝ‖ê + t̂ → ek/sk。
 *
 * 流水线位置：设备侧主类；由 byte_encode12_custom 入口构造并 Process。
 * 与 golden 关系：读 input 契约同 Alg.13 mixPass=7 的 dst/t_hat preset，写出 ek/sk 与 golden_* 对拍。
 * 作用：按 AIV 批加载 NTT preset 与 t̂，对每 poly 调用 poly_byte_encode12_local 后 DataCopy 到 GM。
 */

#include "basic.hpp"
#include "byte_encode12_config.hpp"
#include "kyber_limb6.hpp"
#include "byte_encode12_pair.hpp"
#include "hat_line18_2s1e.hpp"
#include "kernel_operator.h"
#include "tiling.h"

using AscendC::DataCopy;

/**
 * 2×AIV：preset ŝ‖ê + t̂ → ByteEncode₁₂(ek/sk)。
 * 每 AIV 处理 p∈[pBegin,pEnd) 共 2 个 poly：ek←Encode(t̂[p])，sk←Encode(ŝ[p])。
 */
class AivByteEncode12Only {
public:
    /**
     * 构造：绑定子核与系数长度，预计算 UB 长度与 poly 批边界。
     * @param subCoreIdx AIV 子核 0/1
     * @param coeffN     每 poly 系数数（tiling.tileLength，通常 256）
     */
    __aicore__ inline AivByteEncode12Only(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN),
          kPolysS_(static_cast<uint16_t>(tiling::kS)),
          kPolysE_(static_cast<uint16_t>(tiling::kEPerAiv)),
          ubNttLength_(static_cast<uint32_t>(tiling::kS + tiling::kEPerAiv) * coeffN),
          thatTileLength_(static_cast<uint32_t>(tiling::kEPerAiv) * coeffN),
          pBegin_(twos1e::p_begin(subCoreIdx)), pEnd_(twos1e::p_end(subCoreIdx))
    {
    }

    /**
     * 绑定 GM 并分配 UB：ntt preset、t̂ tile、ek/sk 本地 shard + 可选 encode scratch。
     * @param dst_gm  GM int32 dst [12,256]
     * @param t_hat_gm GM int32 t̂ [4,256]
     * @param ek_out  GM uint8 ek polyvec
     * @param sk_out  GM uint8 sk polyvec
     */
    __aicore__ inline void Init(GM_ADDR dst_gm, GM_ADDR t_hat_gm, GM_ADDR ek_out, GM_ADDR sk_out)
    {
        gm_dst_.SetGlobalBuffer((__gm__ int32_t *)dst_gm);
        gm_t_hat_.SetGlobalBuffer((__gm__ int32_t *)t_hat_gm);
        gm_ek_.SetGlobalBuffer((__gm__ uint8_t *)ek_out);
        gm_sk_.SetGlobalBuffer((__gm__ uint8_t *)sk_out);

        // VECIN：ŝ‖ê 与本批 t̂；VECCALC：ek/sk 本地 + 向量编码工作区
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

    /**
     * 主流程：加载 preset → 编码写出 → 释放 UB。
     * 前置条件：已 Init；本核为 AIV。
     */
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
    /**
     * 从 GM dst 拷本 AIV 的 ŝ[0..3] 与 ê 批到 ub_ntt（ŝ 在前，ê 接后）。
     * @param ub_ntt 长度 (kS+kEPerAiv)*coeffN
     * 布局：行 0..3 = ŝ；行 4..5 = 本 AIV ê（来自 dstEOffAiv*）
     */
    __aicore__ inline void loadNttPresetInto(LocalTensor<int32_t> &ub_ntt)
    {
        // 按子核选择 dst 中 ŝ / ê 行偏移（见 tiling.h）
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

    /**
     * 从 GM t_hat 按 poly 批拷入 ub_that（仅本 AIV 的 pBegin..pEnd）。
     * @param ub_that 长度 kEPerAiv*coeffN；local 行 = p-pBegin
     */
    __aicore__ inline void loadThatPresetInto(LocalTensor<int32_t> &ub_that)
    {
        // p：全局 poly 下标；localP：本批内偏移（系数下标）
        for (uint16_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * coeffN_;
            DataCopy(ub_that[localP], gm_t_hat_[static_cast<uint32_t>(p) * coeffN_], coeffN_);
            KYBER_PIPE_ALL();
        }
    }

    /**
     * 对本批每个 poly：Encode(t̂)→ek、Encode(ŝ)→sk，再写回 GM。
     * @param ub_ntt  已加载的 ŝ‖ê
     * @param ub_that 已加载的本批 t̂
     */
    __aicore__ inline void stageEncodeOut(LocalTensor<int32_t> &ub_ntt, LocalTensor<int32_t> &ub_that)
    {
        // encode_ws：向量/prefetch 工作区，紧接在 2×shard 字节缓冲之后
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

        // 对每个本批 poly：先 ek(t̂) 再 sk(ŝ)，本地编码后 DataCopy 到全局 poly 偏移
        for (uint16_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localIdx = static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_);
            const uint32_t byteLocal = localIdx * byte_encode12::kPolyBytes;
            const uint32_t byteGlobal = static_cast<uint32_t>(p) * byte_encode12::kPolyBytes;

            // ek：ByteEncode₁₂(t̂[p])
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

            // sk：ByteEncode₁₂(ŝ[p])；ŝ 行号 = s_row(p)（完整 ŝ 在 ub_ntt 前 kS 行）
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
