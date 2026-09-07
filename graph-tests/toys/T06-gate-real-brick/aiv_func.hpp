#ifndef T06_GATE_REAL_BRICK_AIV_FUNC_HPP
#define T06_GATE_REAL_BRICK_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief AIV 侧：桩哈希半片写 S0 + GATE 段真积木 Vec MAC + INTT 第二轮 1/3 + 完成标记。
 *
 * 背景（T06 / D-EXP-T06）：NTT 1/3 → GATE 4/8（AIC 先 WAIT4，AIV **真 Vec MAC**）→ INTT 复用 1/3（禁 5/7）。
 * 相对 T04：禁止标量 SetValue 假循环加码（X14）；改用 AscendC::Mul + Add 向量乘加。
 * API：Mul/Add/Muls 复用查阅索引既有记录；本刀无新增 API。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * @class AivStubHashSplit
 * @brief 桩哈希 + Stage1 半片写 S0（NTT 段 SET1 前）。
 */
class AivStubHashSplit {
public:
    __aicore__ inline AivStubHashSplit(int32_t subBlockID) : subBlockID_(subBlockID) {}

    __aicore__ inline void Init(GM_ADDR ws)
    {
        const uint32_t s0Off = static_cast<uint32_t>(subBlockID_) * tiling::kS0PerAiv;
        s0GM_.SetGlobalBuffer((__gm__ int8_t *)(ws + tiling::S0 + s0Off), tiling::kS0PerAiv);
        pipe_.InitBuffer(outQ_, 1, tiling::kS0PerAiv * sizeof(int8_t));
    }

    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int8_t> outLocal = outQ_.AllocTensor<int8_t>();
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
 * @class AivGateRealBrickMac
 * @brief GATE 段真积木：有界 int32 Vec MAC（GM↔UB，Mul+Add 向量乘加累加）。
 *
 * 形状：每 AIV a/b/acc 各 [64] int32；kRounds=8；总 512 次向量乘加/AIV。
 * 禁 X14 式标量空转；未采用 SyncAll/SoftSync/抄 Encrypt。
 */
class AivGateRealBrickMac {
public:
    static constexpr uint32_t kMacElems = static_cast<uint32_t>(tiling::kMacElems);
    static constexpr uint32_t kRounds = static_cast<uint32_t>(tiling::kMacRounds);

    __aicore__ inline AivGateRealBrickMac(int32_t subBlockID) : subBlockID_(subBlockID) {}

    __aicore__ inline void Init(GM_ADDR ws)
    {
        const uint32_t off = static_cast<uint32_t>(subBlockID_) * tiling::kMacVecBytes;
        aGM_.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::MAC_A_OFF + off), kMacElems);
        bGM_.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::MAC_B_OFF + off), kMacElems);
        accGM_.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::MAC_ACC_OFF + off), kMacElems);

        pipe_.InitBuffer(aQ_, 1, kMacElems * sizeof(int32_t));
        pipe_.InitBuffer(bQ_, 1, kMacElems * sizeof(int32_t));
        pipe_.InitBuffer(prodQ_, 1, kMacElems * sizeof(int32_t));
        pipe_.InitBuffer(accQ_, 1, kMacElems * sizeof(int32_t));
    }

    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int32_t> acc = accQ_.AllocTensor<int32_t>();
        AscendC::DataCopy(acc, accGM_, kMacElems);

        for (uint32_t round = 0; round < kRounds; ++round) {
            AscendC::LocalTensor<int32_t> a = aQ_.AllocTensor<int32_t>();
            AscendC::LocalTensor<int32_t> b = bQ_.AllocTensor<int32_t>();
            AscendC::LocalTensor<int32_t> prod = prodQ_.AllocTensor<int32_t>();

            AscendC::DataCopy(a, aGM_, kMacElems);
            AscendC::DataCopy(b, bGM_, kMacElems);

            const int32_t scale = static_cast<int32_t>(1 + round + subBlockID_);
            AscendC::Muls(b, b, scale, kMacElems);
            AscendC::Mul(prod, a, b, kMacElems);
            AscendC::Add(acc, acc, prod, kMacElems);

            aQ_.FreeTensor(a);
            bQ_.FreeTensor(b);
            prodQ_.FreeTensor(prod);
            AscendC::PipeBarrier<PIPE_ALL>();
        }

        accQ_.EnQue(acc);
        acc = accQ_.DeQue<int32_t>();
        AscendC::DataCopy(accGM_, acc, kMacElems);
        accQ_.FreeTensor(acc);
    }

private:
    int32_t subBlockID_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> aQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> bQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> prodQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> accQ_;
    AscendC::GlobalTensor<int32_t> aGM_;
    AscendC::GlobalTensor<int32_t> bGM_;
    AscendC::GlobalTensor<int32_t> accGM_;
};

/** @brief INTT WAIT(3) 之后写 32B 完成标记到 out。 */
class AivDoneMark {
public:
    static constexpr uint32_t kMarkElems = 8;

    __aicore__ inline AivDoneMark(int32_t subBlockID) : subBlockID_(subBlockID) {}

    __aicore__ inline void Init(GM_ADDR out)
    {
        const uint32_t off = static_cast<uint32_t>(subBlockID_) * kMarkElems * sizeof(int32_t);
        outGM_.SetGlobalBuffer((__gm__ int32_t *)(out + off), kMarkElems);
        pipe_.InitBuffer(outQ_, 1, kMarkElems * sizeof(int32_t));
    }

    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int32_t> t = outQ_.AllocTensor<int32_t>();
        AscendC::Duplicate(t, static_cast<int32_t>(subBlockID_), kMarkElems);
        t.SetValue(0, static_cast<int32_t>(0x706000 + subBlockID_));
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

#endif /* T06_GATE_REAL_BRICK_AIV_FUNC_HPP */
