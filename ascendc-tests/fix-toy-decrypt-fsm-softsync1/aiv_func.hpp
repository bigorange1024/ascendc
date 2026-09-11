#ifndef FIX_TOY_DECRYPT_FSM_SOFTSYNC1_AIV_FUNC_HPP
#define FIX_TOY_DECRYPT_FSM_SOFTSYNC1_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief AIV 侧：生产同构 SoftSyncArrive + GT-4 风格完成标记 DataCopy。
 *
 * 背景（DGT-20260903-1 / Q-TOY-SOFTSYNC）：隔离 Decrypt 独有 AIV1 忙等；
 * 本玩具要测 SoftSyncArrive 在 SIM 会否挂死。
 * 未采用：用 CrossCore / SyncAll 代替 SoftSync。
 * 禁止 SyncAll（见 J-FAIL-SYNCALL-ON-AIC-WAIT）；本刀亦禁止 CrossCore GATE/NTT。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * SoftSyncArrive：字面同构生产 Decrypt fused_entry。
 *
 * 背景：隔离 Decrypt AIV1 对 softSyncGm 的 while==0 忙等。
 * 结论：本 toy 要测该机制在 SIM 会否 SynchronizeStream 挂死。
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
 * @brief SoftSync 通过后写 32B 完成标记到 out/trace（学 GT-4；禁 Duplicate(int8)）。
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
     * SoftSync 之后写完成标记。
     * 首元素 0xSOFT0 / 0xSOFT1（AIV0/AIV1），便于 Host 区分挂在 busy-wait vs 已跑完。
     */
    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int32_t> t = outQ_.AllocTensor<int32_t>();
        // 完成标记：Duplicate(int32) 填 subBlockID，再 SetValue 块首完成码；禁 Duplicate(int8)
        AscendC::Duplicate(t, static_cast<int32_t>(subBlockID_), kMarkElems);
        // 0x50F700 / 0x50F701：mnemonic SoftSync Finished
        t.SetValue(0, static_cast<int32_t>(0x50F700 + subBlockID_));
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
