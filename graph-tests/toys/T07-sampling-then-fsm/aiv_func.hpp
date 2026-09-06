#ifndef T07_SAMPLING_THEN_FSM_AIV_FUNC_HPP
#define T07_SAMPLING_THEN_FSM_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief AIV 侧：SAMPLE 轻量 stub → S0 写 → GATE 真 Vec MAC → INTT 1/3 → 完成标记。
 *
 * 背景（T07 / D-EXP-T07）：前置 SAMPLE（Host seed + 设备向量 mixing）→ NTT 1/3 → GATE → INTT 1/3。
 * SAMPLE 来源：`library/shared/fips203_se_sample` 同款 SHA3-256 参考（Host gen_data.py）；
 * 设备侧用 AscendC Mul/Add/Muls 有界 mixing，禁 X14 空转、禁抄 Encrypt。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * @class AivSampleStub
 * @brief 轻量 sampling/哈希 stub：读 Host seed[32]，向量 mixing 写 SAMPLE_OUT[64]/AIV。
 *
 * 形状：seed 8×int32；work/out 各 16×int32；4 轮 Muls+Add；输出 64B/AIV 写 GM。
 * 非 SHAKE 全量实现；不对 liboqs/KAT。
 */
class AivSampleStub {
public:
    static constexpr uint32_t kSeedInt32 = 8;
    static constexpr uint32_t kWorkInt32 = 16;
    static constexpr uint32_t kMixRounds = 4;

    __aicore__ inline AivSampleStub(int32_t subBlockID) : subBlockID_(subBlockID) {}

    __aicore__ inline void Init(GM_ADDR ws)
    {
        seedGM_.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::SEED), kSeedInt32);
        const uint32_t outOff =
            static_cast<uint32_t>(subBlockID_) * static_cast<uint32_t>(tiling::kSampleOutPerAiv);
        outGM_.SetGlobalBuffer((__gm__ int8_t *)(ws + tiling::SAMPLE_OUT + outOff),
                               tiling::kSampleOutPerAiv);

        pipe_.InitBuffer(seedQ_, 1, kSeedInt32 * sizeof(int32_t));
        pipe_.InitBuffer(workQ_, 1, kWorkInt32 * sizeof(int32_t));
        pipe_.InitBuffer(tmpQ_, 1, kWorkInt32 * sizeof(int32_t));
        pipe_.InitBuffer(outQ_, 1, tiling::kSampleOutPerAiv);
    }

    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int32_t> seed = seedQ_.AllocTensor<int32_t>();
        AscendC::LocalTensor<int32_t> work = workQ_.AllocTensor<int32_t>();
        AscendC::DataCopy(seed, seedGM_, kSeedInt32);

        AscendC::Duplicate(work, static_cast<int32_t>(0), kWorkInt32);
        AscendC::DataCopy(work, seed, kSeedInt32);
        for (uint32_t i = kSeedInt32; i < kWorkInt32; ++i) {
            work.SetValue(i, seed.GetValue(i - kSeedInt32) ^ static_cast<int32_t>(subBlockID_ + i));
        }

        for (uint32_t round = 0; round < kMixRounds; ++round) {
            AscendC::LocalTensor<int32_t> tmp = tmpQ_.AllocTensor<int32_t>();
            const int32_t scale = static_cast<int32_t>(1 + round + subBlockID_);
            AscendC::Muls(tmp, work, scale, kWorkInt32);
            AscendC::Add(work, work, tmp, kWorkInt32);
            tmpQ_.FreeTensor(tmp);
            AscendC::PipeBarrier<PIPE_ALL>();
        }

        AscendC::LocalTensor<int8_t> outLocal = outQ_.AllocTensor<int8_t>();
        for (uint32_t b = 0; b < tiling::kSampleOutPerAiv; ++b) {
            const uint32_t w = b / sizeof(int32_t);
            const uint32_t shift = (b % sizeof(int32_t)) * 8;
            const int32_t v = work.GetValue(w % kWorkInt32);
            outLocal.SetValue(b, static_cast<int8_t>((v >> shift) & 0xFF));
        }
        outQ_.EnQue(outLocal);
        outLocal = outQ_.DeQue<int8_t>();
        AscendC::DataCopy(outGM_, outLocal, tiling::kSampleOutPerAiv);

        outQ_.FreeTensor(outLocal);
        workQ_.FreeTensor(work);
        seedQ_.FreeTensor(seed);
        AscendC::PipeBarrier<PIPE_ALL>();
    }

private:
    int32_t subBlockID_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> seedQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> workQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> tmpQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ_;
    AscendC::GlobalTensor<int32_t> seedGM_;
    AscendC::GlobalTensor<int8_t> outGM_;
};

/**
 * @class AivStubHashSplit
 * @brief SAMPLE 输出 → Stage1 半片写 S0（NTT 段 SET1 前）。
 */
class AivStubHashSplit {
public:
    __aicore__ inline AivStubHashSplit(int32_t subBlockID) : subBlockID_(subBlockID) {}

    __aicore__ inline void Init(GM_ADDR ws)
    {
        const uint32_t sampleOff =
            static_cast<uint32_t>(subBlockID_) * static_cast<uint32_t>(tiling::kSampleOutPerAiv);
        sampleGM_.SetGlobalBuffer((__gm__ int8_t *)(ws + tiling::SAMPLE_OUT + sampleOff),
                                  tiling::kSampleOutPerAiv);
        const uint32_t s0Off = static_cast<uint32_t>(subBlockID_) * tiling::kS0PerAiv;
        s0GM_.SetGlobalBuffer((__gm__ int8_t *)(ws + tiling::S0 + s0Off), tiling::kS0PerAiv);
        pipe_.InitBuffer(sampleQ_, 1, tiling::kSampleOutPerAiv);
        pipe_.InitBuffer(outQ_, 1, tiling::kS0PerAiv * sizeof(int8_t));
    }

    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int8_t> sample = sampleQ_.AllocTensor<int8_t>();
        AscendC::DataCopy(sample, sampleGM_, tiling::kSampleOutPerAiv);

        AscendC::LocalTensor<int8_t> outLocal = outQ_.AllocTensor<int8_t>();
        for (uint32_t i = 0; i < tiling::kS0PerAiv; ++i) {
            const int8_t base = sample.GetValue(i % tiling::kSampleOutPerAiv);
            outLocal.SetValue(i, static_cast<int8_t>(base + static_cast<int8_t>(subBlockID_)));
        }
        outQ_.EnQue(outLocal);
        outLocal = outQ_.DeQue<int8_t>();
        AscendC::DataCopy(s0GM_, outLocal, tiling::kS0PerAiv);
        outQ_.FreeTensor(outLocal);
        sampleQ_.FreeTensor(sample);
    }

private:
    int32_t subBlockID_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> sampleQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ_;
    AscendC::GlobalTensor<int8_t> sampleGM_;
    AscendC::GlobalTensor<int8_t> s0GM_;
};

/**
 * @class AivGateRealBrickMac
 * @brief GATE 段真积木：有界 int32 Vec MAC（同 T06）。
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
        t.SetValue(0, static_cast<int32_t>(0x707000 + subBlockID_));
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

#endif /* T07_SAMPLING_THEN_FSM_AIV_FUNC_HPP */
