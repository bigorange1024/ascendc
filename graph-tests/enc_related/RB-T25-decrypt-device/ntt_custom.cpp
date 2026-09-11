/**
 * @file ntt_custom.cpp
 * @brief RB-T25 Launch2：MIX — flag 1/3 → û←NTT(u)；ŵ←⟨ŝ,û⟩。
 *
 * AIC：Wait(1)→Cube→Set(3)；AIV：Set(1)→Wait(3)→NTT+dot（AIV0）。
 * 永禁 5/7；Wait 环禁 SyncAll。NTT 与 INTT 分 launch（反卡死 §5.1）。
 */
#include "kernel_operator.h"
#include "light_cube.hpp"
#include "ntt_device_math.hpp"
#include "tiling.h"

__aicore__ inline void TraceMark(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void CrossWait(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void CrossSet(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * @param out [out] 握手魔数（本 launch 不写最终 m）
 * @param ws  [in/out] 含 u/ŝ/ζ/γ；写出 û/ŵ
 */
extern "C" __global__ __aicore__ void ntt_custom(GM_ADDR out, GM_ADDR ws, TilingData tiling)
{
    (void)tiling;
    (void)out;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    using namespace tiling;
    GM_ADDR traceGm = ws + OFF_TRACE;
    GM_ADDR matA = ws + OFF_MAT_A;
    GM_ADDR matB = ws + OFF_MAT_B;
    GM_ADDR matC = ws + OFF_MAT_C_NTT;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        rb_t25::LightCube cube;
        cube.Init();
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_NTT, MAGIC_AIC_POST_WAIT1_NTT);
        cube.Process(matC, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();
        TraceMark(traceGm, SLOT_AIC_PRE_SET3_NTT, MAGIC_AIC_PRE_SET3_NTT);
        CrossSet(kFlagAicDone);
    } else {
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_NTT, MAGIC_AIV0_PRE_SET1_NTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_NTT, MAGIC_AIV1_PRE_SET1_NTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_NTT, MAGIC_AIV0_POST_WAIT3_NTT);
            rb_t25::ComputeNttAndSuDot(ws);
            TraceMark(traceGm, SLOT_AIV0_NTT_DOT_DONE, MAGIC_AIV0_NTT_DOT_DONE);
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3_NTT, MAGIC_AIV1_POST_WAIT3_NTT);
        }
    }
}
