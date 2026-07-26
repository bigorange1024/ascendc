/**
 * @file aiv_func.hpp
 * @brief Encrypt compute 段 AIV（Vector）侧：NTT/INTT 的 Stage1 编码、Stage2 后 Pack、Stage3 RouteA merge。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024 Encrypt 的正向 NTT(y)、逆向 INTT(û/tr̂)
 * 以及 SIM 融合核 `l18_l19` 内同构子段。与 AIC `AicMmad` 通过 CrossCore FSM 握手。
 *
 * 语义约束（poly-batch）：每个 AIV 握完整 poly 的 hi+lo（非 limbsplit）；平面 mat_c；
 * S1–S3 内禁止 Gather。与 golden 关系：仅设备中间态，最终对拍 `output/c.bin`。
 *
 * 三类：
 *   AivK8Split        — Stage1：int32 系数 → S0 int8 [16,256]（HI/LO 分行）
 *   AivK8PackMatCPlanar — Stage2 后：四路 Cube 临时 → 平面 mat_c
 *   AivK8RouteAMod    — Stage3：平面 mat_c → 目标 [k,256]（RouteA + mod q）
 */
#ifndef STAGE123_POLYVEC8_AIV_FUNC_HPP
#define STAGE123_POLYVEC8_AIV_FUNC_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "ntt_vec.hpp"
#include "stage1_config.hpp"
#include "stage3_config.hpp"
#include "tiling.h"

using AscendC::DataCopy;

namespace planar_k8 {

/**
 * 平面 mat_c 行下标：按 (half, slot, limb) 三维展开。
 * @param slot  poly 槽位（0..kPlanarSlots-1）
 * @param limb  limb 索引（0..kLimbsPerPoly-1）
 * @param half  0=lo 半平面，1=hi 半平面
 * @return 行号，再乘 halfLen 得 int32 元素偏移
 */
__aicore__ inline uint32_t mat_row(uint32_t slot, uint32_t limb, uint32_t half)
{
    return half * static_cast<uint32_t>(tiling::kPlanarSlots) * tiling::kLimbsPerPoly +
           slot * tiling::kLimbsPerPoly + limb;
}

} // namespace planar_k8

/**
 * Stage1：把本 AIV 负责的 poly 批从 int32 源编码为 S0 的 HI/LO int8 行。
 * 布局：S0 前 kK 行为各 poly 的 HI limb 行，后 kK 行为 LO；AIV0 握 poly 0..kPolysPerAiv-1。
 */
class AivK8Split {
public:
    /**
     * @param subCoreIdx AIV 子核号（0/1）→ 决定 polyBase_
     * @param coeffN     每 poly 系数个数（通常 256）
     */
    __aicore__ inline AivK8Split(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN),
          polyBase_((subCoreIdx == 0) ? 0U : static_cast<uint32_t>(tiling::kPolysPerAiv))
    {
    }

    /**
     * 绑定 GM：src 为 int32 系数源（y 或 û），wsS0 为 Stage1 输出 int8 矩阵。
     * 分配 VECIN/VECOUT 队列；可选 splitScratch（向量 split 路径）。
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

    /** 从 GM src 读本核 poly 批并编码写 S0。 */
    __aicore__ inline void Process()
    {
        encodeBank(polyBase_, static_cast<uint16_t>(tiling::kPolysPerAiv));
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
     * 核心编码：local_src → split_vec → HI/LO int8 → 按行写 gm_s0_。
     * hi 行写 local_dst1，lo 行写 local_dst0（与 limb6 约定一致）。
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
        // 逐 poly：HI 行 = polyBase+lp；LO 行 = kK + polyBase+lp
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

    /** GM→UB 拷贝本核 poly 批后调用 encodeCore。 */
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
 * Stage2 后：把 AIC 四路临时（lo/hi × even/odd）拼成平面 mat_c，供 Stage3 RouteA。
 * 每 poly 一个 limb tile：hh‖lh‖hl‖ll（各 halfLen=128 int32）。
 */
class AivK8PackMatCPlanar {
public:
    __aicore__ inline AivK8PackMatCPlanar(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx_(subCoreIdx), coeffN_(coeffN), halfLen_(coeffN / 2),
          polyBase_((subCoreIdx == 0) ? 0U : static_cast<uint32_t>(tiling::kPolysPerAiv)),
          limbTileLength_(4 * (coeffN / 2))
    {
    }

    /**
     * 绑定平面输出与四路 Cube 临时 GM。
     * @param matPlanar 平面 mat_c 基址
     * @param tmpLoEven/Odd、tmpHiEven/Odd  对应 AIC 四次 MMAD 写回区
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

    /** 对本 AIV 的 poly 批执行 lo 半平面 + hi 半平面 Pack。 */
    __aicore__ inline void Process()
    {
        packBank(polyBase_, static_cast<uint16_t>(tiling::kPolysPerAiv));
    }

private:
    /**
     * 对给定 half（0=lo / 1=hi）：逐 poly 从 even/odd 临时取 HI/LO 行，拼 4×halfLen 写平面。
     * 索引：hiR/loR 对应 S0 的 HI/LO 行号；slot 用于 mat_row。
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
            // tile 布局：even(hi) ‖ odd(hi) ‖ even(lo) ‖ odd(lo)
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

    /** 先 pack lo 半平面（half=0），再 pack hi 半平面（half=1）。 */
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
 * Stage3：平面 mat_c → 目标系数矩阵（与 vec-k4-v2 Aiv2s1eRouteAMod 同构）。
 * 先处理 lo 半系数（偶奇 deinterleave + limb6 合并 + mod 3329），再处理 hi 半系数，
 * 拼成完整 [poly,256] 后写 GM。S1–S3 内禁止 Gather，合并在 UB 向量路径完成。
 */
class AivK8RouteAMod {
public:
    __aicore__ inline AivK8RouteAMod(int32_t subCoreIdx, uint32_t coeffN)
        : coeffN_(coeffN), halfLen_(coeffN / 2), limbTileLength_(4 * (coeffN / 2)),
          polyBase_((subCoreIdx == 0) ? 0U : static_cast<uint32_t>(tiling::kPolysPerAiv))
    {
    }

    /**
     * @param dst       输出 GM（如 yHat / uOut），[kPolys,256] int32
     * @param matPlanar 平面 mat_c 输入
     * 按 F203_STAGE3_MOD 分配不同 mod 变体的 scratch（0=双 int32，2=float 商）。
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

    /** 对本 AIV poly 批：slotBase=dstPolyOff=polyBase_。 */
    __aicore__ inline void Process()
    {
        ProcessBank(polyBase_, polyBase_, static_cast<uint16_t>(tiling::kPolysPerAiv));
    }

private:
    /**
     * 一批 poly 的 Stage3：读 lo 平面 → 合并写 local_dst[0:half)；再读 hi 平面 → [half:N)；最后 copyOut。
     * @param slotBase   平面 mat_c 的 slot 起点
     * @param dstPolyOff 输出 GM 的 poly 行起点
     * @param kPolys     本批 poly 数
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
        // ---- lo 半系数（系数下标 0..127）----
        DataCopy(plane, gm_planar_[loPlaneBase], batchHalfPlaneLength);
        KYBER_PIPE_ALL();

        LocalTensor<int32_t> half_out = tmp_half_.AllocTensor<int32_t>();
        for (uint16_t lp = 0; lp < kPolys; ++lp) {
            const uint32_t outBase = static_cast<uint32_t>(lp) * coeffN_;
            const uint32_t limbOff = static_cast<uint32_t>(lp) * limbTileLength_;
            LocalTensor<int32_t> limbs = limb_scratch_.AllocTensor<int32_t>();
            DataCopy(limbs, plane[limbOff], limbTileLength_);
            KYBER_PIPE_ALL();
            // hh/lh/hl/ll：hi-even / hi-odd / lo-even / lo-odd 四路 limb
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

        // ---- hi 半系数（系数下标 128..255）写到 local_dst[outBase+halfLen_] ----
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

    /** 将 UB 中拼好的 [kPolys,256] 按 poly 行写回 gm_dst_。 */
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
