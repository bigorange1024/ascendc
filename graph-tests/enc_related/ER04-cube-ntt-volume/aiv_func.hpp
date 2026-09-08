#ifndef ER04_CUBE_NTT_VOLUME_AIV_FUNC_HPP
#define ER04_CUBE_NTT_VOLUME_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief AIV 侧：Launch1 prep 桩 → Launch2 S0 写 / GATE 真 Vec MAC / INTT / 完成标记。
 *
 * 背景（ER04 / D-EXP-ER04）：自 ER03 壳复制；GATE 保持 kMacElems=256、kMacRounds=32；
 * AIC 侧 Cube 加压在 mmad_custom（NTT/INTT 各 kCubeRounds=16）。本文件同步纪律不回退。
 * 禁 X14 空转、禁抄 Encrypt、禁 SoftSync、禁擅自缩小 MAC / 改 Cube 几何。
 *
 * ER02 同步纪律（本刀保持，目标 sync_audit 红线 0）：
 *   - GM→UB 后统一 EnQue/DeQue（MTE2→V/S），禁止 Alloc+DataCopy 后直接算/标量读；
 *   - Duplicate/Muls/Add（V）后若接 SetValue/GetValue（S），插 PipeBarrier<PIPE_V>；
 *   - 输入队列用 VECIN、输出用 VECOUT（与 cannbot 标准三段流水一致）。
 * 未采用：否决红线；HardEvent SetFlag（本刀优先 EnQue/DeQue + PIPE_V）。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * @class AivSampleStub
 * @brief Launch1 轻量 prep 桩：读 Host seed[32]，向量 mixing 写 SAMPLE_OUT[64]/AIV。
 *
 * 形状：seed 8×int32；work/out 各 16×int32；4 轮 Muls+Add；输出 64B/AIV 写 GM。
 * 非 SHAKE 全量；可不对 liboqs。可 Host 喂表代替真采样。
 */
class AivSampleStub {
public:
    static constexpr uint32_t kSeedInt32 = 8;
    static constexpr uint32_t kWorkInt32 = 16;
    static constexpr uint32_t kMixRounds = 4;

    __aicore__ inline AivSampleStub(int32_t subBlockID) : subBlockID_(subBlockID) {}

    /**
     * 绑定 GM 与 UB 队列。
     * @param ws workspace 基址（含 SEED / SAMPLE_OUT）
     */
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

    /** 读 seed → 4 轮 Muls+Add mixing → 写 SAMPLE_OUT。 */
    __aicore__ inline void Process()
    {
        // CopyIn seed：DataCopy 后 EnQue/DeQue，清 MTE2→V/S（SYNC-02）
        AscendC::LocalTensor<int32_t> seed = seedQ_.AllocTensor<int32_t>();
        AscendC::DataCopy(seed, seedGM_, kSeedInt32);
        seedQ_.EnQue(seed);
        seed = seedQ_.DeQue<int32_t>();

        AscendC::LocalTensor<int32_t> work = workQ_.AllocTensor<int32_t>();
        AscendC::Duplicate(work, static_cast<int32_t>(0), kWorkInt32);
        // V→S：随后 Scalar 填尾 / 读 seed 前同步
        AscendC::PipeBarrier<PIPE_V>();
        AscendC::DataCopy(work, seed, kSeedInt32);
        // UB 内 DataCopy 仍记 MTE2_write；计算前再 EnQue/DeQue
        workQ_.EnQue(work);
        work = workQ_.DeQue<int32_t>();
        for (uint32_t i = kSeedInt32; i < kWorkInt32; ++i) {
            work.SetValue(i, seed.GetValue(i - kSeedInt32) ^ static_cast<int32_t>(subBlockID_ + i));
        }
        // S→V：标量填尾后接 Muls/Add
        AscendC::PipeBarrier<PIPE_ALL>();

        for (uint32_t round = 0; round < kMixRounds; ++round) {
            AscendC::LocalTensor<int32_t> tmp = tmpQ_.AllocTensor<int32_t>();
            const int32_t scale = static_cast<int32_t>(1 + round + subBlockID_);
            AscendC::Muls(tmp, work, scale, kWorkInt32);
            AscendC::Add(work, work, tmp, kWorkInt32);
            tmpQ_.FreeTensor(tmp);
            AscendC::PipeBarrier<PIPE_ALL>();
        }

        // V→S：打包 out 前 GetValue
        AscendC::PipeBarrier<PIPE_V>();
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
    AscendC::TQue<AscendC::TPosition::VECIN, 1> seedQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> workQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> tmpQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ_;
    AscendC::GlobalTensor<int32_t> seedGM_;
    AscendC::GlobalTensor<int8_t> outGM_;
};

/**
 * @class AivStubHashSplit
 * @brief Launch2：SAMPLE_OUT → Stage1 半片写 S0（NTT SET1 前）。
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
        // CopyIn sample → EnQue/DeQue 后再 Scalar GetValue（SYNC-02）
        AscendC::LocalTensor<int8_t> sample = sampleQ_.AllocTensor<int8_t>();
        AscendC::DataCopy(sample, sampleGM_, tiling::kSampleOutPerAiv);
        sampleQ_.EnQue(sample);
        sample = sampleQ_.DeQue<int8_t>();

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
    AscendC::TQue<AscendC::TPosition::VECIN, 1> sampleQ_;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ_;
    AscendC::GlobalTensor<int8_t> sampleGM_;
    AscendC::GlobalTensor<int8_t> s0GM_;
};

/**
 * @class AivGateRealBrickMac
 * @brief GATE 段真积木：近生产体量 int32 Vec MAC（256×32；禁 X14 假循环）。
 *
 * UB：a/b/prod/acc 各 256×int32 ≈ 4×1KiB；单缓冲 TQue，与 ER02 同形。
 * 若编译/UB 放不下 → 停止改参，回报主控（ER04-TASK）。
 */
class AivGateRealBrickMac {
public:
    static constexpr uint32_t kMacElems = static_cast<uint32_t>(tiling::kMacElems);
    static constexpr uint32_t kRounds = static_cast<uint32_t>(tiling::kMacRounds);

    __aicore__ inline AivGateRealBrickMac(int32_t subBlockID) : subBlockID_(subBlockID) {}

    /**
     * 绑定每 AIV 的 MAC_A/B/ACC GM 切片，并 InitBuffer 四路 TQue。
     * @param ws workspace 基址（偏移见 tiling::MAC_*_OFF）
     */
    __aicore__ inline void Init(GM_ADDR ws)
    {
        // 每 AIV 独占 kMacVecBytes；双 AIV 在 GM 上连续排布
        const uint32_t off = static_cast<uint32_t>(subBlockID_) * tiling::kMacVecBytes;
        aGM_.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::MAC_A_OFF + off), kMacElems);
        bGM_.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::MAC_B_OFF + off), kMacElems);
        accGM_.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::MAC_ACC_OFF + off), kMacElems);

        pipe_.InitBuffer(aQ_, 1, kMacElems * sizeof(int32_t));
        pipe_.InitBuffer(bQ_, 1, kMacElems * sizeof(int32_t));
        pipe_.InitBuffer(prodQ_, 1, kMacElems * sizeof(int32_t));
        pipe_.InitBuffer(accQ_, 1, kMacElems * sizeof(int32_t));
    }

    /**
     * 每轮：读 a/b → Muls(b,scale) → Mul → Add 累加；写回 acc。
     * 体量已锁 256×32，真 Mul/Add/Muls，非空转加码。
     * ER02：每路 DataCopy 后 EnQue/DeQue，再进 V 计算；写回前 EnQue/DeQue。
     */
    __aicore__ inline void Process()
    {
        // CopyIn acc：GM→UB 后 EnQue/DeQue，清 MTE2→V（SYNC-02）
        AscendC::LocalTensor<int32_t> acc = accQ_.AllocTensor<int32_t>();
        AscendC::DataCopy(acc, accGM_, kMacElems);
        accQ_.EnQue(acc);
        acc = accQ_.DeQue<int32_t>();

        for (uint32_t round = 0; round < kRounds; ++round) {
            AscendC::LocalTensor<int32_t> a = aQ_.AllocTensor<int32_t>();
            AscendC::LocalTensor<int32_t> b = bQ_.AllocTensor<int32_t>();
            AscendC::LocalTensor<int32_t> prod = prodQ_.AllocTensor<int32_t>();

            // 每轮重读 a/b（真积木体量；非 X14 空 for）
            AscendC::DataCopy(a, aGM_, kMacElems);
            aQ_.EnQue(a);
            a = aQ_.DeQue<int32_t>();

            AscendC::DataCopy(b, bGM_, kMacElems);
            bQ_.EnQue(b);
            b = bQ_.DeQue<int32_t>();

            // 真 Vec MAC：Muls → Mul → Add（PIPE_V）
            const int32_t scale = static_cast<int32_t>(1 + round + subBlockID_);
            AscendC::Muls(b, b, scale, kMacElems);
            AscendC::Mul(prod, a, b, kMacElems);
            AscendC::Add(acc, acc, prod, kMacElems);

            aQ_.FreeTensor(a);
            bQ_.FreeTensor(b);
            prodQ_.FreeTensor(prod);
            AscendC::PipeBarrier<PIPE_ALL>();
        }

        // CopyOut acc：V→MTE3 经 EnQue/DeQue（标准 VECOUT 形）
        accQ_.EnQue(acc);
        acc = accQ_.DeQue<int32_t>();
        AscendC::DataCopy(accGM_, acc, kMacElems);
        accQ_.FreeTensor(acc);
    }

private:
    int32_t subBlockID_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> aQ_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> bQ_;
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
        // V→S：Duplicate 后 SetValue 前（SYNC-02）
        AscendC::PipeBarrier<PIPE_V>();
        t.SetValue(0, static_cast<int32_t>(0xE01000 + subBlockID_));
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

#endif /* ER04_CUBE_NTT_VOLUME_AIV_FUNC_HPP */
