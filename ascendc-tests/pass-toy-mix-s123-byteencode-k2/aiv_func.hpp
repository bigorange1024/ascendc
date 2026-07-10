#ifndef TOY_MIX_S123_AIV_FUNC_HPP
#define TOY_MIX_S123_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief 双 AIV 向量核上的 Stage1（limb+填数）与 Stage3（Adds+func1+写 out）。
 *
 * MIX 拓扑：KERNEL_TYPE_MIX_AIC_1_2 → 1 个 Cube + 2 个 Vector（subBlockID 0/1）。
 * 本文件不涉及跨 AIV 同步；两核仅通过 GM 上不相交的地址区间协作。
 *
 * 分片约定（方案 B，见 TOY_MIX_S123.md）：
 *   flat 下标 i 对应行优先 64×64 矩阵 A[i//64, i%64]
 *   AIV0 负责 i ∈ [0, 2048)，AIV1 负责 i ∈ [2048, 4096)
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#include "toy_encode.hpp"

/**
 * @class AivToySplit
 * @brief Stage1：模拟 AivSplitPolyBatch 的最小版本。
 *
 * 数据流（每 AIV 独立）：
 *   GM src[1024 int32]  →  UB  →  玩具 limb  →  覆盖填数 i%128  →  GM ws+S0 半片[2048 int8]
 *
 * 玩具 limb（非真 Kyber limb6）：
 *   每个 int32 系数 v 拆成 lo = v&0x3f、hi = (v>>6)&0x3f 各 1 字节；
 *   布局为 [lo₀..lo₁₀₂₃ | hi₀..hi₁₀₂₃]，共 2048 int8。
 *   输入全 0 时 limb 结果亦全 0，随后被填数步骤整体覆盖（与 golden 一致）。
 *
 * 填数规则：
 *   baseIdx = subBlockID * 2048
 *   outLocal[i] = (baseIdx + i) % 128
 *   两片拼成完整 A，满足 golden A[i] = i % 128。
 */
class AivToySplit {
public:
    __aicore__ inline AivToySplit(int32_t subBlockID) : subBlockID_(subBlockID) {}

    /**
     * @param ws  workspace 基址（写 ws+S0+偏移）
     * @param src 输入多项式系数基址（读 subBlockID 对应的 1024 int32 片）
     */
    __aicore__ inline void Init(GM_ADDR ws, GM_ADDR src)
    {
        // src 按 int32 切半：AIV0 @0，AIV1 @4096 字节
        const uint32_t srcOff = static_cast<uint32_t>(subBlockID_) * tiling::kSrcPerAiv * sizeof(int32_t);
        // S0 按 int8 切半：AIV0 @0，AIV1 @2048
        const uint32_t s0Off = static_cast<uint32_t>(subBlockID_) * tiling::kOutPerAiv;
        srcGM_.SetGlobalBuffer((__gm__ int32_t *)(src + srcOff), tiling::kSrcPerAiv);
        s0GM_.SetGlobalBuffer((__gm__ int8_t *)(ws + tiling::S0 + s0Off), tiling::kOutPerAiv);

        pipe_.InitBuffer(inQ_, 1, tiling::kSrcPerAiv * sizeof(int32_t));
        pipe_.InitBuffer(outQ_, 1, tiling::kOutPerAiv * sizeof(int8_t));
    }

    /**
     * 执行 Stage1 完整流水：CopyIn（GM→UB 读 src 半片）→ 玩具 limb 拆分
     * → 填数覆盖 → CopyOut（UB→GM 写 ws+S0 半片）。
     * 前置条件：须在 Init() 之后调用；无返回值，直接写入构造时绑定的 GM 区间。
     */
    __aicore__ inline void Process()
    {
        // ---- GM → UB：读本核 src 半片 ----
        AscendC::LocalTensor<int32_t> srcLocal = inQ_.AllocTensor<int32_t>();
        AscendC::DataCopy(srcLocal, srcGM_, tiling::kSrcPerAiv);
        inQ_.EnQue(srcLocal);
        srcLocal = inQ_.DeQue<int32_t>();

        AscendC::LocalTensor<int8_t> outLocal = outQ_.AllocTensor<int8_t>();
        const uint32_t baseIdx = static_cast<uint32_t>(subBlockID_) * tiling::kOutPerAiv;

        // ---- 玩具 limb：1024 int32 → 2048 int8（lo||hi）----
        for (uint32_t i = 0; i < tiling::kSrcPerAiv; ++i) {
            int32_t v = srcLocal.GetValue(i);
            int8_t lo = static_cast<int8_t>(v & 0x3f);
            int8_t hi = static_cast<int8_t>((v >> 6) & 0x3f);
            outLocal.SetValue(i, lo);
            outLocal.SetValue(tiling::kSrcPerAiv + i, hi);
        }

        // ---- 填数：覆盖 limb 结果，按全局 flat 下标循环 0..127 ----
        for (uint32_t i = 0; i < tiling::kOutPerAiv; ++i) {
            outLocal.SetValue(i, static_cast<int8_t>((baseIdx + i) % 128));
        }

        // ---- UB → GM：写 ws+S0 本核半片 ----
        outQ_.EnQue(outLocal);
        outLocal = outQ_.DeQue<int8_t>();
        AscendC::DataCopy(s0GM_, outLocal, tiling::kOutPerAiv);
        outQ_.FreeTensor(outLocal);
        inQ_.FreeTensor(srcLocal);
    }

private:
    int32_t subBlockID_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ_;
    AscendC::GlobalTensor<int32_t> srcGM_;
    AscendC::GlobalTensor<int8_t> s0GM_;
};

/**
 * @class AivToyS3Encode
 * @brief Stage3 + encode：对应真流程 S3 向量加与 ByteEncode₁₂。
 *
 * UB 驻留约定：
 *   1. 允许入口一次 DataCopy：GM MAT_C 半片 → UB（本探针唯一允许的 S3 侧 GM 读）
 *   2. Adds(+1)、func1 均在 UB 完成
 *   3. 出口一次 DataCopy：UB int8 → GM out 半片
 *   禁止 Adds 与 func1 之间把 int32 中间结果写回 GM。
 *
 * Golden：out[i] = (C[i] + 1) % 64，其中 C = A @ I = A（int32 扩宽累加，无溢出）。
 */
class AivToyS3Encode {
public:
    __aicore__ inline AivToyS3Encode(int32_t subBlockID) : subBlockID_(subBlockID) {}

    /**
     * @param out   最终输出基址（写 int8 半片）
     * @param ws    workspace 基址（读 ws+MAT_C 半片）
     */
    __aicore__ inline void Init(GM_ADDR out, GM_ADDR ws)
    {
        const uint32_t off = static_cast<uint32_t>(subBlockID_) * tiling::kOutPerAiv * sizeof(int32_t);
        const uint32_t outOff = static_cast<uint32_t>(subBlockID_) * tiling::kOutPerAiv;
        matCGM_.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::MAT_C + off), tiling::kOutPerAiv);
        outGM_.SetGlobalBuffer((__gm__ int8_t *)(out + outOff), tiling::kOutPerAiv);

        pipe_.InitBuffer(cInQ_, 1, tiling::kOutPerAiv * sizeof(int32_t));
        pipe_.InitBuffer(cMidQ_, 1, tiling::kOutPerAiv * sizeof(int32_t));
        pipe_.InitBuffer(outQ_, 1, tiling::kOutPerAiv * sizeof(int8_t));
    }

    /**
     * 执行 Stage3+encode 完整流水：入口 DataCopy（MAT_C 半片 → UB）→ Adds(+1)
     * → func1（%64→int8，均在 UB 内完成）→ 出口 DataCopy（UB → GM out 半片）。
     * 前置条件：须在 Init() 之后调用；须在 AIC 完成 MAT_C 写入并通过 CrossCore
     * SET(AIC_MMAD) 通知后才能调用（由 mmad_custom.cpp 中的 WAIT 保证）。
     */
    __aicore__ inline void Process()
    {
        // ---- 入口：MAT_C 半片 → UB（int32）----
        AscendC::LocalTensor<int32_t> cLocal = cInQ_.AllocTensor<int32_t>();
        AscendC::DataCopy(cLocal, matCGM_, tiling::kOutPerAiv);
        cInQ_.EnQue(cLocal);
        cLocal = cInQ_.DeQue<int32_t>();

        // ---- UB：逐元素 +1（模拟 Stage3 模加前的标量偏移）----
        AscendC::LocalTensor<int32_t> plusLocal = cMidQ_.AllocTensor<int32_t>();
        AscendC::Adds(plusLocal, cLocal, static_cast<int32_t>(1), tiling::kOutPerAiv);
        cMidQ_.EnQue(plusLocal);
        cInQ_.FreeTensor(cLocal);

        // ---- UB：func1（%64 → int8），仍在 UB 内 ----
        plusLocal = cMidQ_.DeQue<int32_t>();
        AscendC::LocalTensor<int8_t> outLocal = outQ_.AllocTensor<int8_t>();
        toy::Func1Mod64(outLocal, plusLocal, tiling::kOutPerAiv);
        outQ_.EnQue(outLocal);
        cMidQ_.FreeTensor(plusLocal);

        // ---- 出口：UB → GM out 半片 ----
        outLocal = outQ_.DeQue<int8_t>();
        AscendC::DataCopy(outGM_, outLocal, tiling::kOutPerAiv);
        outQ_.FreeTensor(outLocal);
    }

private:
    int32_t subBlockID_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> cInQ_;   /**< MAT_C 读入 */
    AscendC::TQue<AscendC::TPosition::VECIN, 1> cMidQ_;  /**< Adds 结果 */
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ_;  /**< func1 结果 */
    AscendC::GlobalTensor<int32_t> matCGM_;
    AscendC::GlobalTensor<int8_t> outGM_;
};

#endif
