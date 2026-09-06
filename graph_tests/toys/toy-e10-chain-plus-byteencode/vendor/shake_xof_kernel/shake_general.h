/**
 * @file shake_general.h
 * @brief SHAKE XOF 设备核：I/O 均为 AscendC LocalTensor（UB），rate 由 tiling 区分 128/256。
 *
 * ## Process vs ProcessInline
 * - `Process()`：独立 launch 时用 GetBlockIdx/GetBlockNum 划分 batch 组（多核 SHAKE toy 等）。
 * - `ProcessInline()`：被其它多 block 核函数内嵌调用；当前 AIV 独占 UB batch，固定 blockIdx=0、blockNum=1。
 *   禁止在内嵌路径使用 Process()，否则外层 block1 + batch=1 时 Keccak 空转（KeyGen Opt-4，见 docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md）。
 * staging32 须 ≥32B 且 32B 对齐，供块拼包；块 I/O 用 uint64（SIM 上 UB DataCopy 后 GetValue 不可见）。
 * 派生自 ops-math shake128_general.h。
 */
#pragma once

#include "kernel_operator.h"
#include "keccak_f1600.h"
#include "shake_general_tiling_data.h"

namespace ShakeXofKernel {

/** KernelShakeGeneral::Init 所需 UB 暂存块长度（DataCopy 最小粒度）。 */
constexpr uint32_t SHAKE_XOF_STAGING_BYTES = 32U;

class KernelShakeGeneral {
public:
    __aicore__ inline KernelShakeGeneral() {}

    /**
     * @param x          消息批 [batch, maxMsgLen] uint8，行优先
     * @param lengths    每条有效长度 [batch] uint32
     * @param y          XOF 输出 [batch, outLen] uint8，行优先
     * @param staging32  ≥32B 对齐暂存，供 StoreBlock32 拼包
     * @param tiling     batch/maxMsgLen/outLen/rate/groupSize
     */
    __aicore__ inline void Init(AscendC::LocalTensor<uint8_t> x, AscendC::LocalTensor<uint32_t> lengths,
                                AscendC::LocalTensor<uint8_t> y, AscendC::LocalTensor<uint8_t> staging32,
                                const ShakeGeneralTilingData *tiling)
    {
        x_ = x;
        lengths_ = lengths;
        y_ = y;
        staging32_ = staging32;
        batch_ = tiling->batch;
        maxMsgLen_ = tiling->maxMsgLen;
        outLen_ = tiling->outLen;
        rate_ = tiling->rate;
        groupSize_ = tiling->groupSize;
    }

    /**
     * 独立 launch 多核分片：按 GetBlockIdx/GetBlockNum 划分 batch 组（shake toy 大 batch 等）。
     */
    __aicore__ inline void Process()
    {
        const uint32_t blockIdx = AscendC::GetBlockIdx();
        const uint32_t realBlockNum = AscendC::GetBlockNum();
        ProcessWithBlockRange(blockIdx, realBlockNum);
    }

    /**
     * 内嵌于其它多 block 核函数时调用：本 AIV 独占 UB 上整批 batch，不受外层 blockIdx 影响。
     * 背景：a_hat16 Opt-4 blockDim=2 时，外层 block1 若沿用 GetBlockIdx()==1，batch=1 的 SHAKE 会空转（qa/2026-06-25 Opt-4）。
     */
    __aicore__ inline void ProcessInline()
    {
        ProcessWithBlockRange(0U, 1U);
    }

private:
    __aicore__ inline void ProcessWithBlockRange(uint32_t blockIdx, uint32_t realBlockNum)
    {
        const uint32_t groupCount = (batch_ + groupSize_ - 1U) / groupSize_;

        for (uint32_t groupIdx = blockIdx; groupIdx < groupCount; groupIdx += realBlockNum) {
            const uint32_t begin = groupIdx * groupSize_;
            uint32_t end = begin + groupSize_;
            if (end > batch_) {
                end = batch_;
            }

            for (uint32_t msgIdx = begin; msgIdx < end; ++msgIdx) {
                ProcessOne(msgIdx);
            }
        }
    }

private:
    __aicore__ inline uint64_t LoadPartialLaneUb(const AscendC::LocalTensor<uint8_t> &t, uint32_t base, uint32_t n)
    {
        uint64_t v = 0;
        for (uint32_t i = 0; i < n; ++i) {
            v |= static_cast<uint64_t>(t.GetValue(base + i)) << (8U * i);
        }
        return v;
    }

    __aicore__ inline uint64_t Load64LeUb(const AscendC::LocalTensor<uint8_t> &t, uint32_t base)
    {
        return LoadPartialLaneUb(t, base, 8U);
    }

    /** 从 Keccak 状态 a[] 按字节偏移拼 8B 小端字。 */
    __aicore__ inline uint64_t PackState8ToU64(const uint64_t a[25], uint32_t byteOffset)
    {
        uint64_t w = 0;
        for (uint32_t j = 0; j < 8U; ++j) {
            const uint32_t pos = byteOffset + j;
            const uint32_t lane = pos / 8U;
            const uint32_t shift = (pos % 8U) * 8U;
            w |= static_cast<uint64_t>((a[lane] >> shift) & 0xffU) << (8U * j);
        }
        return w;
    }

    /**
     * 吸收 32B：按 uint64 块 xor（4×/32B，替代 32× 字节 GetValue）。
     * 背景：SIM 上 UB→UB DataCopy 后标量 GetValue 不可见（qa/2026-06-23 §12）；吸收侧不用 DataCopy。
     */
    __aicore__ inline void XorBlock32(uint64_t a[25], uint32_t &lane, uint32_t msgBase, uint32_t pos)
    {
        AscendC::LocalTensor<uint64_t> x64 = x_[msgBase + pos].ReinterpretCast<uint64_t>();
        for (uint32_t i = 0; i < 4U && lane < 25U; ++i) {
            a[lane] ^= x64.GetValue(i);
            ++lane;
        }
    }

    /**
     * 挤出 32B：拼 4×uint64 写 y（替代 32× 字节 SetValue）。
     * staging32_ 作拼包缓冲，经 uint64 写 y 以保证 SIM 上 GetValue 可见。
     */
    __aicore__ inline void StoreBlock32(uint32_t outBase, const uint64_t a[25], uint32_t byteOffset)
    {
        AscendC::LocalTensor<uint64_t> st64 = staging32_.ReinterpretCast<uint64_t>();
        AscendC::LocalTensor<uint64_t> y64 = y_[outBase].ReinterpretCast<uint64_t>();
        for (uint32_t word = 0; word < 4U; ++word) {
            const uint64_t w = PackState8ToU64(a, byteOffset + word * 8U);
            st64.SetValue(word, w);
            y64.SetValue(word, w);
        }
    }

    __aicore__ inline void XorBytes(uint64_t a[25], uint32_t msgBase, uint32_t offset, uint32_t n)
    {
        uint32_t lane = 0;
        uint32_t pos = offset;

        while (n >= SHAKE_XOF_STAGING_BYTES) {
            XorBlock32(a, lane, msgBase, pos);
            pos += SHAKE_XOF_STAGING_BYTES;
            n -= SHAKE_XOF_STAGING_BYTES;
        }

        if ((maxMsgLen_ % 8U) == 0U) {
            while (n >= 8U) {
                a[lane] ^= Load64LeUb(x_, msgBase + pos);
                pos += 8U;
                n -= 8U;
                ++lane;
            }
        } else {
            while (n >= 8U) {
                a[lane] ^= LoadPartialLaneUb(x_, msgBase + pos, 8U);
                pos += 8U;
                n -= 8U;
                ++lane;
            }
        }

        if (n > 0U) {
            a[lane] ^= LoadPartialLaneUb(x_, msgBase + pos, n);
        }
    }

    __aicore__ inline void StoreOutputBytes(uint32_t outBase, const uint64_t a[25], uint32_t offset, uint32_t n)
    {
        uint32_t done = 0U;
        while (done + SHAKE_XOF_STAGING_BYTES <= n) {
            StoreBlock32(outBase + done, a, offset + done);
            done += SHAKE_XOF_STAGING_BYTES;
        }

        for (uint32_t i = done; i < n; ++i) {
            const uint32_t pos = offset + i;
            const uint32_t lane = pos / 8U;
            const uint32_t shift = (pos % 8U) * 8U;
            y_.SetValue(outBase + i, static_cast<uint8_t>((a[lane] >> shift) & 0xffU));
        }
    }

    __aicore__ inline void ProcessOne(uint32_t msgIdx)
    {
        const uint32_t msgBase = msgIdx * maxMsgLen_;
        const uint32_t outBase = msgIdx * outLen_;

        uint32_t msgLen = lengths_.GetValue(msgIdx);
        if (msgLen > maxMsgLen_) {
            msgLen = maxMsgLen_;
        }

        uint64_t a[25];
        for (int i = 0; i < 25; ++i) {
            a[i] = 0;
        }

        uint32_t offset = 0;
        while (offset + rate_ <= msgLen) {
            XorBytes(a, msgBase, offset, rate_);
            KeccakF1600Kernel::PermuteChain(a);
            offset += rate_;
        }

        const uint32_t rem = msgLen - offset;
        XorBytes(a, msgBase, offset, rem);

        const uint32_t suffixLane = rem / 8U;
        const uint32_t suffixShift = (rem % 8U) * 8U;
        a[suffixLane] ^= static_cast<uint64_t>(0x1fU) << suffixShift;

        const uint32_t padPos = rate_ - 1U;
        const uint32_t padLane = padPos / 8U;
        const uint32_t padShift = (padPos % 8U) * 8U;
        a[padLane] ^= static_cast<uint64_t>(0x80U) << padShift;

        KeccakF1600Kernel::PermuteChain(a);

        uint32_t produced = 0;
        while (produced < outLen_) {
            uint32_t chunk = outLen_ - produced;
            if (chunk > rate_) {
                chunk = rate_;
            }
            StoreOutputBytes(outBase + produced, a, 0, chunk);
            produced += chunk;
            if (produced < outLen_) {
                KeccakF1600Kernel::PermuteChain(a);
            }
        }
    }

private:
    AscendC::LocalTensor<uint8_t> x_;
    AscendC::LocalTensor<uint32_t> lengths_;
    AscendC::LocalTensor<uint8_t> y_;
    AscendC::LocalTensor<uint8_t> staging32_;

    uint32_t batch_ = 0;
    uint32_t maxMsgLen_ = 0;
    uint32_t outLen_ = 0;
    uint32_t rate_ = SHAKE128_RATE_BYTES;
    uint32_t groupSize_ = 1;
};

}  // namespace ShakeXofKernel
