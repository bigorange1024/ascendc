#ifndef PASS_TOY_DECRYPT_FSM_FUSED_SKEL1_AIV_FUNC_HPP
#define PASS_TOY_DECRYPT_FSM_FUSED_SKEL1_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief AIV 侧：生产同构 SoftSyncArrive/Clear + 完成标记 DataCopy。
 *
 * 背景（DGT-20260903-3 / Q-TOY-FUSED-SKEL）：Decrypt fused_entry 双 SoftSync 槽；
 * SoftSync 本体字面同构生产（不可用 CrossCore 代替）。
 * 结论：本刀 SoftSync×2 夹两轮 GATE，再叠 NTT/INTT 1/3。
 * 未采用：用 CrossCore/SyncAll 代替 SoftSync；Encrypt 单 GATE 序；INTT flag 5/7。
 * TRACE 写槽在 mmad_custom.cpp 的 ToyTraceMark（GT-4 DataCopy）。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * SoftSyncArrive：字面同构生产 Decrypt fused_entry。
 *
 * 背景：隔离 Decrypt AIV1 对 softSyncGm 的 while==0 忙等（F-SOFTSYNC-ARRIVE）。
 * 结论：slot0=prep、slot1=su；仅 AIV0 写 1，AIV1 忙等。
 * 未采用：用 CrossCore / SyncAll 代替 SoftSync。
 *
 * @param softSyncGm  Host 已 H2D 清零的哨兵 GM（≥64B，int32 槽）
 * @param slot        0=prep / 1=su
 * @param subBlockID  0=AIV0 写 1；非 0=AIV1 忙等
 */
__aicore__ inline void SoftSyncArrive(GM_ADDR softSyncGm, int32_t slot, int32_t subBlockID)
{
    auto *s = reinterpret_cast<__gm__ int32_t *>(softSyncGm);
    if (subBlockID == 0) {
        s[slot] = 1;
        AscendC::PipeBarrier<PIPE_ALL>();
    } else {
        while (s[slot] == 0) {
        }
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}

/**
 * SoftSyncClear：AIV0 清哨兵，供同 launch 内下一段复用 / 收尾。
 *
 * 背景：生产 fused_entry 在 GATE WAIT(8) 后 Clear，再开 NTT/INTT。
 * 结论：仅 AIV0 写 0 + PipeBarrier；AIV1 不参与。
 * 未采用：双核都清；用 SyncAll 代替 Clear。
 *
 * @param softSyncGm / slot / subBlockID 同 SoftSyncArrive
 */
__aicore__ inline void SoftSyncClear(GM_ADDR softSyncGm, int32_t slot, int32_t subBlockID)
{
    if (subBlockID == 0) {
        reinterpret_cast<__gm__ int32_t *>(softSyncGm)[slot] = 0;
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}

/**
 * @class AivDoneMark
 * @brief 全链路末尾写 32B 完成标记到 out（DataCopy 对齐；不对正确性）。
 */
class AivDoneMark {
public:
    static constexpr uint32_t kMarkElems = 8; /**< 8×int32=32B */

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

    /**
     * 完成标记：首元素 0xF05E00 / 0xF05E01（mnemonic Fused Soft+GATE+NTT/INTT Ended）。
     */
    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int32_t> t = outQ_.AllocTensor<int32_t>();
        AscendC::Duplicate(t, static_cast<int32_t>(subBlockID_), kMarkElems);
        t.SetValue(0, static_cast<int32_t>(0xF05E00 + subBlockID_));
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
