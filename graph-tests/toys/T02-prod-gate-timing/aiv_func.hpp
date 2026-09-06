#ifndef T02_PROD_GATE_TIMING_AIV_FUNC_HPP
#define T02_PROD_GATE_TIMING_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief AIV 侧：桩哈希半片写 S0 + 轻体量 scratch + 完成标记写 out。
 *
 * 背景（T02 / D-EXP-T02）：NTT 同构 1/3 后，AIV 在 GATE 段做有界轻体量
 * （UB 向量循环 + 少量 DataCopy），再双 AIV SET(4)；禁止 SyncAll / SoftSync / INTT。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * @class AivStubHashSplit
 * @brief 桩哈希 + Stage1 半片写 S0（NTT 段 SET1 前）。
 *
 * 数据流（每 AIV 独立，地址不相交）：
 *   SetValue(常数) → UB int8[256] → DataCopy → GM ws+S0 半片
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
     * 桩哈希：标量填常数到 UB，写本核 S0 半片。
     * 前置：Init 已完成；无 SyncAll。
     */
    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int8_t> outLocal = outQ_.AllocTensor<int8_t>();
        // 桩哈希：常数 7+subBlockID；禁 SHAKE；不用 Duplicate(int8)
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
 * @class AivLightWorkload
 * @brief GATE 段轻体量：有界 UB 向量循环 + GM scratch 读写，模拟内积/at_jp 前段。
 *
 * 设计约束（T02 TASK）：勿巨型 MMAD；单次 SIM ≤5min。本类固定 4 轮 × 256 int8。
 * 背景：生产路径 AIV 在 SET(4) 前做真实体量；toy 用轻循环占位，时序语义不变。
 * 未采用：对称 GATE（双方齐到再 SET4）；Wait 中 SyncAll。
 *
 * @param ws workspace；读写 ws+WORK 本核半片
 */
class AivLightWorkload {
public:
    static constexpr uint32_t kRounds = 4; /**< 有界循环轮数，足够拉开与 AIC WAIT4 的时间差 */

    __aicore__ inline AivLightWorkload(int32_t subBlockID) : subBlockID_(subBlockID) {}

    __aicore__ inline void Init(GM_ADDR ws)
    {
        const uint32_t off = static_cast<uint32_t>(subBlockID_) * tiling::kWorkPerAiv;
        workGM_.SetGlobalBuffer((__gm__ int8_t *)(ws + tiling::WORK + off), tiling::kWorkPerAiv);
        pipe_.InitBuffer(vecQ_, 1, tiling::kWorkPerAiv * sizeof(int8_t));
    }

    /**
     * 轻体量：GM→UB→逐元素 +1→UB→GM，重复 kRounds 轮。
     * 前置：NTT WAIT(3) 已完成；本核独立 scratch，无 AIV↔AIV SoftSync。
     */
    __aicore__ inline void Process()
    {
        for (uint32_t round = 0; round < kRounds; ++round) {
            AscendC::LocalTensor<int8_t> t = vecQ_.AllocTensor<int8_t>();
            // 从 scratch 读入 UB
            AscendC::DataCopy(t, workGM_, tiling::kWorkPerAiv);
            // 标量 +1 模拟少量向量活（Duplicate 不支持 int8）
            for (uint32_t i = 0; i < tiling::kWorkPerAiv; ++i) {
                const int8_t v = t.GetValue(i);
                t.SetValue(i, static_cast<int8_t>(v + static_cast<int8_t>(1 + round)));
            }
            vecQ_.EnQue(t);
            t = vecQ_.DeQue<int8_t>();
            AscendC::DataCopy(workGM_, t, tiling::kWorkPerAiv);
            vecQ_.FreeTensor(t);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
    }

private:
    int32_t subBlockID_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> vecQ_;
    AscendC::GlobalTensor<int8_t> workGM_;
};

/**
 * @class AivDoneMark
 * @brief WAIT(8) 之后写 32B 完成标记到 out（DataCopy 对齐；不对正确性）。
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
        // 完成标记：首元素 0xT02xx，其余填 subBlockID；非算法 oracle
        AscendC::Duplicate(t, static_cast<int32_t>(subBlockID_), kMarkElems);
        t.SetValue(0, static_cast<int32_t>(0x702000 + subBlockID_));
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

#endif /* T02_PROD_GATE_TIMING_AIV_FUNC_HPP */
