#ifndef FIX_TOY_DECRYPT_FSM_SOFT_GATE1_AIV_FUNC_HPP
#define FIX_TOY_DECRYPT_FSM_SOFT_GATE1_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief AIV 侧：生产同构 SoftSyncArrive + GT-4 风格完成标记 DataCopy。
 *
 * 背景（DGT-20260903-2 / Q-TOY-SOFT-GATE）：SoftSyncArrive 后再一轮 GATE 4↔8；
 * SoftSync 本体仍字面同构生产（不可用 CrossCore 代替）。
 * 未采用：用 CrossCore / SyncAll 代替 SoftSync；第二轮 GATE；NTT/INTT。
 * 禁止 SyncAll（见 J-FAIL-SYNCALL-ON-AIC-WAIT）。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * SoftSyncArrive：字面同构生产 Decrypt fused_entry。
 *
 * 背景：隔离 Decrypt AIV1 对 softSyncGm 的 while==0 忙等（F-SOFTSYNC-ARRIVE）。
 * 结论：本 toy 在 SoftSync 汇合后再接 GATE，测 SoftSync+GATE 融合是否挂死。
 * 未采用：用 CrossCore / SyncAll 代替 SoftSync。
 *
 * @param softSyncGm  Host 已 H2D 清零的哨兵 GM（≥64B，int32 槽）
 * @param slot        本刀固定 0（F-SOFTSYNC-ARRIVE slot0=prep）
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
 * @class AivDoneMark
 * @brief SoftSync+GATE 通过后写 32B 完成标记到 out/trace（学 GT-4；禁 Duplicate(int8)）。
 *
 * 布局：逻辑槽 slot → GM 偏移 slot*8 个 int32；块首元素置完成码。
 * AIV：VECOUT UB 上 Duplicate(int32)+SetValue → DataCopy 到槽块。
 */
class AivDoneMark {
public:
    static constexpr uint32_t kMarkElems = tiling::kTraceAlignInts; /**< 8×int32=32B */

    __aicore__ inline AivDoneMark(int32_t subBlockID) : subBlockID_(subBlockID) {}

    /**
     * @param out GM 输出/TRACE；AIV0→slot0，AIV1→slot1
     */
    __aicore__ inline void Init(GM_ADDR out)
    {
        const uint32_t slot = static_cast<uint32_t>(subBlockID_);
        const uint32_t offInts = slot * kMarkElems;
        outGM_.SetGlobalBuffer((__gm__ int32_t *)(out) + offInts, kMarkElems);
        pipe_.InitBuffer(outQ_, 1, kMarkElems * sizeof(int32_t));
    }

    /**
     * SoftSync+GATE 之后写完成标记。
     * 首元素 0x5F6800 / 0x5F6801（AIV0/AIV1），便于 Host 区分挂点。
     * 0x5F68 mnemonic Soft+Gate Finished。
     */
    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int32_t> t = outQ_.AllocTensor<int32_t>();
        // 完成标记：Duplicate(int32) 填 subBlockID，再 SetValue 块首完成码；禁 Duplicate(int8)
        AscendC::Duplicate(t, static_cast<int32_t>(subBlockID_), kMarkElems);
        t.SetValue(0, static_cast<int32_t>(0x5F6800 + subBlockID_));
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
