#ifndef FIX_TOY_ENCRYPT_FSM_NTT1_AIV_FUNC_HPP
#define FIX_TOY_ENCRYPT_FSM_NTT1_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief AIV 侧：桩「哈希」（禁止真 SHAKE）+ 半片写 S0 + 完成标记写 out。
 *
 * 背景（GT-20260903-1）：Encrypt 任务链 μ 前缀有 SHAKE；本玩具用 Duplicate
 * 常数填充代替，只保留「写完左矩阵半片 → SET(1)」的握手意图。
 * 禁止 SyncAll / SoftSync（见 J-FAIL-SYNCALL-SOFTSYNC、D-NO-SYNCALL-WHILE-AIC-WAIT）。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * @class AivStubHashSplit
 * @brief 桩哈希 + Stage1 半片写 S0。
 *
 * 数据流（每 AIV 独立，地址不相交）：
 *   Duplicate(常数) → UB int8[256] → DataCopy → GM ws+S0 半片
 */
class AivStubHashSplit {
public:
    __aicore__ inline AivStubHashSplit(int32_t subBlockID) : subBlockID_(subBlockID) {}

    /**
     * @param ws workspace 基址（写 S0 半片）
     */
    __aicore__ inline void Init(GM_ADDR ws)
    {
        const uint32_t s0Off = static_cast<uint32_t>(subBlockID_) * tiling::kS0PerAiv;
        s0GM_.SetGlobalBuffer((__gm__ int8_t *)(ws + tiling::S0 + s0Off), tiling::kS0PerAiv);
        pipe_.InitBuffer(outQ_, 1, tiling::kS0PerAiv * sizeof(int8_t));
    }

    /**
     * 桩哈希：标量填常数到 UB（Duplicate 不支持 int8，见 API 约束），写本核 S0 半片。
     * 前置：Init 已完成；无 SyncAll。
     */
    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int8_t> outLocal = outQ_.AllocTensor<int8_t>();
        // 桩哈希：常数 7+subBlockID，禁止 SHAKE/Keccak；不用 Duplicate(int8)
        const int8_t stubVal = static_cast<int8_t>(7 + subBlockID_);
        for (uint32_t i = 0; i < tiling::kS0PerAiv; ++i) {
            outLocal.SetValue(i, stubVal);
        }
        outQ_.EnQue(outLocal);
        outLocal = outQ_.DeQue<int8_t>();
        AscendC::DataCopy(s0GM_, outLocal, tiling::kS0PerAiv);
        outQ_.FreeTensor(outLocal);
    }

private:
    int32_t subBlockID_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ_;
    AscendC::GlobalTensor<int8_t> s0GM_;
};

/**
 * @class AivDoneMark
 * @brief WAIT(3) 之后写 32B 完成标记到 out（DataCopy 对齐；不对正确性）。
 */
class AivDoneMark {
public:
    static constexpr uint32_t kMarkElems = 8; /**< 8×int32=32B，满足搬运对齐 */

    __aicore__ inline AivDoneMark(int32_t subBlockID) : subBlockID_(subBlockID) {}

    /**
     * @param out GM 输出；AIV0 写 [0:32)，AIV1 写 [32:64)
     */
    __aicore__ inline void Init(GM_ADDR out)
    {
        const uint32_t off = static_cast<uint32_t>(subBlockID_) * kMarkElems * sizeof(int32_t);
        outGM_.SetGlobalBuffer((__gm__ int32_t *)(out + off), kMarkElems);
        pipe_.InitBuffer(outQ_, 1, kMarkElems * sizeof(int32_t));
    }

    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int32_t> t = outQ_.AllocTensor<int32_t>();
        // 完成标记：首元素 0xA11Exx，其余填 subBlockID；非算法 oracle
        AscendC::Duplicate(t, static_cast<int32_t>(subBlockID_), kMarkElems);
        t.SetValue(0, static_cast<int32_t>(0xA11E00 + subBlockID_));
        outQ_.EnQue(t);
        t = outQ_.DeQue<int32_t>();
        AscendC::DataCopy(outGM_, t, kMarkElems);
        outQ_.FreeTensor(t);
    }

private:
    int32_t subBlockID_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ_;
    AscendC::GlobalTensor<int32_t> outGM_;
};

#endif
