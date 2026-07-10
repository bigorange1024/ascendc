#pragma once

/**
 * @file f203_encrypt_intt_stage.hpp
 * @brief Alg.14 行 19/21：uTr[5] pad→k=8 的 INTT Stage1–3（MIX 几何对齐 stage123 polyvec8）。
 *
 * UB 分片（INTT S1 输入，每 AIV 4 行）：
 *   AIV0: uTr[0], uTr[1], uTr[4], 0
 *   AIV1: uTr[2], uTr[3], 0, 0
 *
 * Stage3 输出 scatter：
 *   AIV0 local0→u[0], local1→u[1], local2→v（行 21）；AIV1 local0→u[2], local1→u[3]。
 */
#include "aiv_func.hpp"
#include "f203_l18_l19_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

namespace encrypt_intt {

constexpr uint32_t kInttBatch = static_cast<uint32_t>(tiling::kInttBatch);
constexpr uint32_t kInttPolysPerAiv = static_cast<uint32_t>(tiling::kInttPolysPerAiv);

namespace planar_intt {

/**
 * INTT 平面 mat_c 行号（kInttBatch=8 槽）。
 * 与 planar_k8::mat_row 同构，但 half 维乘 kInttBatch 而非 NTT kK。
 */
__aicore__ inline uint32_t mat_row(uint32_t slot, uint32_t limb, uint32_t half)
{
    return half * kInttBatch * tiling::kLimbsPerPoly + slot * tiling::kLimbsPerPoly + limb;
}

} // namespace planar_intt

/**
 * INTT S1：k=8 几何；loRow 基准为 kInttBatch（非 NTT 的 kK）。
 * 输入：ProcessFromLocal 的 pad-8 UB；输出：S0 [16,N] int8。
 */
class AivInttK8Split {
public:
    /**
     * @param subCoreIdx AIV 0/1；@param coeffN 通常 256
     * polyBase_：AIV0→0，AIV1→4（每核 4 行 pad-8）
     */
    __aicore__ inline AivInttK8Split(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN),
          polyBase_((subCoreIdx == 0) ? 0U : kInttPolysPerAiv)
    {
    }

    /**
     * 仅绑定 S0 并分配 lo/hi 输出队列（及可选 splitScratch）。
     * @param wsS0 Stage1 输出；src 保留签名兼容，融合路径不读 GM
     */
    __aicore__ inline void Init(GM_ADDR wsS0, GM_ADDR /*src*/)
    {
        gm_s0_.SetGlobalBuffer((__gm__ int8_t *)wsS0);
        const uint32_t inBytes = kInttPolysPerAiv * coeffN_ * sizeof(int32_t);
        const uint32_t outBytes = kInttPolysPerAiv * coeffN_ * sizeof(int8_t);
        pipe_.InitBuffer(inQ_, 1, inBytes);
        pipe_.InitBuffer(out0_, 1, outBytes);
        pipe_.InitBuffer(out1_, 1, outBytes);
#if F203_STAGE1_SPLIT >= 1
        const uint32_t maxBank = kInttPolysPerAiv * coeffN_;
        const uint32_t scratchBytes = maxBank * (3U * sizeof(int32_t) + sizeof(int16_t) + sizeof(half));
        pipe_.InitBuffer(splitScratch_, scratchBytes);
#endif
    }

    /**
     * uTr pad-8 已在 UB；不经 GM 绕路。
     * @param local_src [kInttPolysPerAiv,N] int32，与本核分片对齐
     */
    __aicore__ inline void ProcessFromLocal(AscendC::LocalTensor<int32_t> &local_src)
    {
        encodeCore(polyBase_, static_cast<uint16_t>(kInttPolysPerAiv), local_src);
    }

private:
    /**
     * limb6 分裂 → 写 S0：hi 行 [polyBase..)；lo 行 [kInttBatch+polyBase..)。
     * 与 NTT AivK8Split 的差异：lo 基准用 kInttBatch=8，不是 kK=4。
     */
    __aicore__ inline void encodeCore(uint32_t polyBase, uint16_t kPolys, AscendC::LocalTensor<int32_t> &local_src)
    {
        AscendC::LocalTensor<int8_t> local_dst0 = out0_.AllocTensor<int8_t>();
        AscendC::LocalTensor<int8_t> local_dst1 = out1_.AllocTensor<int8_t>();
        Tensor_int8x4 res{local_dst0, local_dst1};
        // —— Stage1 split：int32 → lo(x0)/hi(x1) int8 ——
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
        // —— 按 poly 写回 S0（hi 在上半、lo 在下半，间距 kInttBatch）——
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t hiRow = polyBase + static_cast<uint32_t>(lp);
            const uint32_t loRow = kInttBatch + polyBase + static_cast<uint32_t>(lp);
            const uint32_t locOff = static_cast<uint32_t>(lp) * coeffN_;
            AscendC::DataCopy(gm_s0_[hiRow * coeffN_], local_dst1[locOff], coeffN_);
            AscendC::DataCopy(gm_s0_[loRow * coeffN_], local_dst0[locOff], coeffN_);
            AscendC::PipeBarrier<PIPE_ALL>();
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
    AscendC::GlobalTensor<int8_t> gm_s0_;
};

/** INTT S2 后 pack：k=8 平面 mat_c（loRow 用 kInttBatch）。 */
class AivInttK8PackMatCPlanar {
public:
    __aicore__ inline AivInttK8PackMatCPlanar(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN), halfLen_(coeffN / 2),
          polyBase_((subCoreIdx == 0) ? 0U : kInttPolysPerAiv),
          limbTileLength_(4 * (coeffN / 2))
    {
    }

    /** @param matPlanar 平面输出；其余为 AIC 四路临时 */
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

    /** 打包本核 4 个 slot 的 lo/hi 半平面 */
    __aicore__ inline void Process()
    {
        packBank(polyBase_, static_cast<uint16_t>(kInttPolysPerAiv));
    }

private:
    /**
     * 每 poly：从 even/odd 临时取 hi/lo 行各 halfLen，拼 [hh|lh|hl|ll] 写入平面。
     * @param half 0=偶半平面 / 1=奇半平面；lo 行偏移用 kInttBatch
     */
    __aicore__ inline void packPolyHalf(uint32_t polyBase, uint16_t kPolys, uint32_t half,
                                        AscendC::GlobalTensor<int32_t> &gmEven,
                                        AscendC::GlobalTensor<int32_t> &gmOdd)
    {
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t hiR = polyBase + static_cast<uint32_t>(lp);
            const uint32_t loR = kInttBatch + polyBase + static_cast<uint32_t>(lp);
            const uint32_t slot = polyBase + static_cast<uint32_t>(lp);
            AscendC::LocalTensor<int32_t> tile = que_limb_tile_.AllocTensor<int32_t>();
            // tile：[hh | lh | hl | ll]，供 Stage3 RouteA Horner
            AscendC::DataCopy(tile[0], gmEven[hiR * halfLen_], halfLen_);
            AscendC::DataCopy(tile[halfLen_], gmOdd[hiR * halfLen_], halfLen_);
            AscendC::DataCopy(tile[2 * halfLen_], gmEven[loR * halfLen_], halfLen_);
            AscendC::DataCopy(tile[3 * halfLen_], gmOdd[loR * halfLen_], halfLen_);
            KYBER_PIPE_ALL();
            const uint32_t dstBase = planar_intt::mat_row(slot, 0U, half) * halfLen_;
            AscendC::DataCopy(gm_out_[dstBase], tile, limbTileLength_);
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
    uint32_t limbTileLength_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_limb_tile_;
    AscendC::GlobalTensor<int32_t> gm_out_;
    AscendC::GlobalTensor<int32_t> gm_lo_even_, gm_lo_odd_, gm_hi_even_, gm_hi_odd_;
};

/**
 * INTT S3：平面 mat_c → u[4] + v 分片写回。
 * AIV0: local2 为 tr̂ 时域 → vOut；AIV1 仅写 u[2..3]。
 */
class AivInttK8RouteUV {
public:
    __aicore__ inline AivInttK8RouteUV(int32_t subCoreIdx, uint32_t coeffN)
        : coeffN_(coeffN), halfLen_(coeffN / 2), limbTileLength_(4 * (coeffN / 2)),
          polyBase_((subCoreIdx == 0) ? 0U : kInttPolysPerAiv), subCoreIdx_(subCoreIdx)
    {
    }

    /**
     * @param uOut u[4,N]；@param vOut v[N]（仅 AIV0 写）；@param matPlanar 平面 mat_c
     */
    __aicore__ inline void Init(GM_ADDR uOut, GM_ADDR vOut, GM_ADDR matPlanar)
    {
        gm_u_.SetGlobalBuffer((__gm__ int32_t *)uOut);
        gm_v_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(vOut));
        gm_planar_.SetGlobalBuffer((__gm__ int32_t *)matPlanar);
        const uint32_t maxTile = kInttPolysPerAiv * coeffN_ * sizeof(int32_t);
        const uint32_t maxPlane = kInttPolysPerAiv * limbTileLength_ * sizeof(int32_t);
        pipe_.InitBuffer(out_dst_, 1, maxTile);
        pipe_.InitBuffer(scratch_t1_, 1, halfLen_ * sizeof(int32_t));
#if F203_STAGE3_MOD == 0
        pipe_.InitBuffer(scratch_t2_, 1, halfLen_ * sizeof(int32_t));
#endif
        pipe_.InitBuffer(half_plane_, 1, maxPlane);
        pipe_.InitBuffer(limb_scratch_, 1, limbTileLength_ * sizeof(int32_t));
        pipe_.InitBuffer(tmp_half_, 1, halfLen_ * sizeof(int32_t));
    }

    /** RouteA 合并本核 4 行 → scatter 到 u/v（跳过 pad 零行） */
    __aicore__ inline void Process()
    {
        ProcessBank(polyBase_, static_cast<uint16_t>(kInttPolysPerAiv));
    }

private:
    /**
     * 先 lo 半平面再 hi 半平面：每 poly Horner+mod → local_dst[lp] 满 N 系数，再 scatterOut。
     */
    __aicore__ inline void ProcessBank(uint32_t slotBase, uint16_t kPolys)
    {
        const uint32_t batchHalfPlaneLength = static_cast<uint32_t>(kPolys) * limbTileLength_;

        AscendC::LocalTensor<int32_t> local_dst = out_dst_.AllocTensor<int32_t>();
        AscendC::LocalTensor<int32_t> t1 = scratch_t1_.AllocTensor<int32_t>();
#if F203_STAGE3_MOD == 0
        AscendC::LocalTensor<int32_t> t2 = scratch_t2_.AllocTensor<int32_t>();
#endif

        AscendC::LocalTensor<int32_t> plane = half_plane_.AllocTensor<int32_t>();
        const uint32_t loPlaneBase = planar_intt::mat_row(slotBase, 0U, 0U) * halfLen_;
        const uint32_t hiPlaneBase = planar_intt::mat_row(slotBase, 0U, 1U) * halfLen_;
        // —— lo 半：系数 [0,halfN) ——
        AscendC::DataCopy(plane, gm_planar_[loPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();

        AscendC::LocalTensor<int32_t> half_out = tmp_half_.AllocTensor<int32_t>();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN_;
            const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength_;
            AscendC::LocalTensor<int32_t> limbs = limb_scratch_.AllocTensor<int32_t>();
            AscendC::DataCopy(limbs, plane[limbOff], limbTileLength_);
            KYBER_PIPE_ALL();
            LocalTensor<int32_t> hh = limbs[0];
            LocalTensor<int32_t> lh = limbs[halfLen_];
            LocalTensor<int32_t> hl = limbs[2 * halfLen_];
            LocalTensor<int32_t> ll = limbs[3 * halfLen_];
#if F203_STAGE3_MOD == 0
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, t2, 3329, static_cast<int32_t>(halfLen_));
#else
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, t1, 3329, static_cast<int32_t>(halfLen_));
#endif
            KYBER_PIPE_ALL();
            AscendC::DataCopy(local_dst[outBase], half_out, halfLen_);
            KYBER_PIPE_ALL();
            limb_scratch_.FreeTensor(limbs);
        }

        // —— hi 半：系数 [halfN,N) ——
        AscendC::DataCopy(plane, gm_planar_[hiPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN_;
            const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength_;
            AscendC::LocalTensor<int32_t> limbs = limb_scratch_.AllocTensor<int32_t>();
            AscendC::DataCopy(limbs, plane[limbOff], limbTileLength_);
            KYBER_PIPE_ALL();
            LocalTensor<int32_t> hh = limbs[0];
            LocalTensor<int32_t> lh = limbs[halfLen_];
            LocalTensor<int32_t> hl = limbs[2 * halfLen_];
            LocalTensor<int32_t> ll = limbs[3 * halfLen_];
#if F203_STAGE3_MOD == 0
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, t2, 3329, static_cast<int32_t>(halfLen_));
#else
            combine_limb6_routea_mod_vec(half_out, hh, lh, hl, ll, t1, t1, 3329, static_cast<int32_t>(halfLen_));
#endif
            KYBER_PIPE_ALL();
            AscendC::DataCopy(local_dst[outBase + halfLen_], half_out, halfLen_);
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
        scatterOut(kPolys);
    }

    /**
     * 将 local_dst[lp] scatter 到 u / v GM（跳过 pad 行 lp=3 及 AIV1 的 lp≥2）。
     * AIV0: lp0/1→u[0]/u[1]，lp2→v；AIV1: lp0/1→u[2]/u[3]。
     */
    __aicore__ inline void scatterOut(uint16_t kPolys)
    {
        AscendC::LocalTensor<int32_t> local_dst = out_dst_.DeQue<int32_t>();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t locOff = static_cast<uint32_t>(lp) * coeffN_;
            if (subCoreIdx_ == 0) {
                if (lp == 0 || lp == 1) {
                    const uint32_t uRow = static_cast<uint32_t>(lp);
                    AscendC::DataCopy(gm_u_[uRow * coeffN_], local_dst[locOff], coeffN_);
                } else if (lp == 2) {
                    AscendC::DataCopy(gm_v_[0], local_dst[locOff], coeffN_);
                }
                // lp==3 为零 pad，不写
            } else {
                if (lp == 0) {
                    AscendC::DataCopy(gm_u_[2U * coeffN_], local_dst[locOff], coeffN_);
                } else if (lp == 1) {
                    AscendC::DataCopy(gm_u_[3U * coeffN_], local_dst[locOff], coeffN_);
                }
            }
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        out_dst_.FreeTensor(local_dst);
    }

    uint32_t coeffN_;
    uint32_t halfLen_;
    uint32_t limbTileLength_;
    uint32_t polyBase_;
    int32_t subCoreIdx_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t1_;
#if F203_STAGE3_MOD == 0
    AscendC::TQue<AscendC::TPosition::VECIN, 1> scratch_t2_;
#endif
    AscendC::TQue<AscendC::TPosition::VECIN, 1> half_plane_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> limb_scratch_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> tmp_half_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> out_dst_;
    AscendC::GlobalTensor<int32_t> gm_planar_, gm_u_, gm_v_;
};

} // namespace encrypt_intt
