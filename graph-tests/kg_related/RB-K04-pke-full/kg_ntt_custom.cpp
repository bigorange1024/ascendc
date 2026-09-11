/** RB-K04 拼装：本文件自 RB-K02 接线，basename 锁定；算法未改；助手函数 static 防同库 ODR。 */
/**
 * @file kg_ntt_custom.cpp
 * @brief RB-K04 L2a：MIX 设备 NTT(ŝ)+NTT(ê) — 握手(4/1/3) → AIV0 Alg.9×8 poly。
 *
 * KeyGen L2a 外形（KB §B2）：
 *   AIC：Wait(4)→Wait(1)→Cube→Set(3)
 *   AIV：Set(4)→Set(1)→Wait(3) → NTT(ŝ)、NTT(ê)（仅 AIV0）
 *
 * 背景：K04 Host mid-sync 后消费 L1 的 ŝ/ê；头文件走 k02_inc 隔离。
 * 结论：flag ∈ {1,3,4}；永禁 5/7 / SoftSync；与 L1/L2b 同 binary 三 launch。
 */
#include "kernel_operator.h"
#include "k02_inc/light_cube.hpp"
#include "k02_inc/ntt_device_math.hpp"
#include "k02_inc/tiling.h"

/** TRACE 标量写（CAModel 上 AIC/部分 AIV 对 GlobalTensor::SetValue 可能不落盘）。 */
__aicore__ static inline void TraceMark(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** CrossCore Wait：mode 0x2，PIPE_MTE2（910B 上 pipe 模板不生效）。 */
__aicore__ static inline void CrossWait(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** CrossCore Set：mode 0x2，PIPE_MTE2。 */
__aicore__ static inline void CrossSet(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2；blockDim=1。
 * @param out     [out] 握手成功魔数（AIV0 写 4B；非业务 GM SetValue）
 * @param sNttOut [out] 设备算出的 NTT(ŝ)（int32，4096B）
 * @param eNttOut [out] 设备算出的 NTT(ê)（int32，4096B）
 * @param ws      [in/out] 预喂 ŝ/ê/ζ + mat + TRACE；设备写 S_NTT/E_NTT
 * @param tiling  [in] 占位
 */
extern "C" __global__ __aicore__ void kg_ntt_custom(GM_ADDR out, GM_ADDR sNttOut, GM_ADDR eNttOut,
                                                    GM_ADDR ws, TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    using namespace tiling;
    GM_ADDR traceGm = ws + OFF_TRACE;
    GM_ADDR matA = ws + OFF_MAT_A;
    GM_ADDR matB = ws + OFF_MAT_B;
    GM_ADDR matC = ws + OFF_MAT_C;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        rb_k02::LightCube cube;
        cube.Init();

        // ---------- GATE + NTT 握手：Wait(4) → Wait(1) → Cube → Set(3) ----------
        CrossWait(kFlagGate);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT4, MAGIC_AIC_POST_WAIT4);

        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1, MAGIC_AIC_POST_WAIT1);

        cube.Process(matC, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3, MAGIC_AIC_PRE_SET3);
        CrossSet(kFlagAicDone);
    } else {
        // ---------- AIV：Set(4) → Set(1) → Wait(3) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET4, MAGIC_AIV0_PRE_SET4);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET4, MAGIC_AIV1_PRE_SET4);
        }
        CrossSet(kFlagGate);

        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1, MAGIC_AIV0_PRE_SET1);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1, MAGIC_AIV1_PRE_SET1);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3, MAGIC_AIV0_POST_WAIT3);
            // ---------- AIV0：NTT(ŝ)、NTT(ê)（poly-batch；禁 Gather）----------
            rb_k02::ComputeSeNtt(ws, sNttOut, eNttOut);
            TraceMark(traceGm, SLOT_AIV0_DONE, MAGIC_AIV0_DONE);

            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3, MAGIC_AIV1_POST_WAIT3);
        }
    }
}
