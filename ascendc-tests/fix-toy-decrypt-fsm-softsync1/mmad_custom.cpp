/**
 * @file mmad_custom.cpp
 * @brief fix-toy-decrypt-fsm-softsync1：仅 SoftSyncArrive（AIV0 写 / AIV1 忙等）。
 *
 * 图谱：Q-TOY-SOFTSYNC；定式见 F-SOFTSYNC-ARRIVE / F-HOST-ZERO-SOFTSYNC。
 * 形态：KERNEL_TYPE_MIX_AIC_1_2；本刀无 CrossCore GATE/NTT/INTT。
 *
 *   双 AIV：SoftSyncArrive(slot=0) —— AIV0 s[0]=1；AIV1 while(s[0]==0)
 *   SoftSync 后：双 AIV DataCopy 完成标记到 out/trace（GT-4：8×int32 槽）
 *   AIC：无 WaitFlag；极轻 MMAD 16×32×32 后返回；禁 SyncAll
 *
 * 不对算法正确性；验收只认 SIM 进程正常结束且 Host 见完成标记。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2，单 block。
 * @param out         [out] TRACE/完成标记（每槽 32B；slot0=AIV0，slot1=AIV1）
 * @param ws          [in/out] S0/LUT/MAT_C；S0+LUT 由 Host 预填，AIC 独立消费
 * @param softSyncGm  [in/out] SoftSync 哨兵；Host 已 H2D 清零；本刀只用 slot0
 * @param tiling      占位（本玩具无分段）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR softSyncGm,
                                                  TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    // GetSubBlockNum()==1 → AIC；否则 AIV，subBlockIdx 区分 0/1
    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (aic) {
        // ---- AIC：本刀不要 WaitFlag；极轻 Cube 后返回；禁 SyncAll ----
        // 背景：J-FAIL-SYNCALL-ON-AIC-WAIT —— 本刀甚至不进入 Wait，避免误用 SyncAll
        AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                     static_cast<uint16_t>(tiling::kCols));
        mmad.Init();
        mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
        KYBER_PIPE_ALL();
        return;
    }

    // ---- AIV：生产同构 SoftSyncArrive(slot0)，再写完成标记 ----
    // 背景：隔离 Decrypt AIV1 忙等（F-SOFTSYNC-ARRIVE / J-PRIMARY-SOFTSYNC）。
    // 结论：本 toy 要测 SoftSyncArrive 在 SIM 会否挂死。
    // 未采用：用 CrossCore / SyncAll 代替 SoftSync。
    SoftSyncArrive(softSyncGm, tiling::kSoftSyncSlot, subBlockID);
    KYBER_PIPE_ALL();

    // SoftSync 已通过：AIV0/AIV1 各写一槽完成标记（GT-4 DataCopy；禁 Duplicate(int8)）
    {
        AivDoneMark mark(subBlockID);
        mark.Init(out);
        mark.Process();
        KYBER_PIPE_ALL();
    }
}
