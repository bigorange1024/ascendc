/**
 * @file dec_intt_extract_custom.cpp
 * @brief RB-D03：MIX 设备 INTT(ŵ)+extract → m — 握手(4/1/3) → AIV0 Alg.10+Alg.15 尾。
 *
 * Decrypt L2b 外形（S0A）：
 *   AIC：Wait(4)→Wait(1)→Cube→Set(3)
 *   AIV：Set(4)→Set(1)→Wait(3) → w←INTT(ŵ)；m←ByteEncode₁(Compress₁(v−w))（仅 AIV0）
 *
 * 背景：D02 写出 ŵ；D01 写出 v；本刀独立 gen_data 亦可造合法输入（禁 #include D01/D02 源码）。
 * 结论：flag ∈ {1,3,4}；永禁 5/7 / SoftSync；主验收 m[32] 对拍；禁 NTT∥INTT 同核（X15）。
 * 模式：CrossCore modeId=0x2；Host 单 launch + SynchronizeStream；BLOCK_DIM=1。
 *
 * 若挂死 TRACE 假设：
 *   - 无 POST_WAIT3 → 卡握手
 *   - 有 POST_WAIT3 无 DONE → 卡 INTT / extract / 写出
 */
#include "kernel_operator.h"
#include "light_cube.hpp"
#include "intt_extract_device_math.hpp"
#include "tiling.h"

/** TRACE 标量写（CAModel 上 AIC/部分 AIV 对 GlobalTensor::SetValue 可能不落盘）。 */
__aicore__ inline void TraceMark(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** CrossCore Wait：mode 0x2，PIPE_MTE2（910B 上 pipe 模板不生效）。 */
__aicore__ inline void CrossWait(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** CrossCore Set：mode 0x2，PIPE_MTE2。 */
__aicore__ inline void CrossSet(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2；blockDim=1。
 * @param out  [out] 握手成功魔数（AIV0 写 4B；仅 TRACE/状态，非业务 GM SetValue）
 * @param mOut [out] 设备算出的 m（uint8[32]）；Host 禁预填最终值
 * @param ws   [in/out] 预喂 ŵ/v/ζ + mat + TRACE；设备写 W
 * @param tiling [in] 占位
 */
extern "C" __global__ __aicore__ void dec_intt_extract_custom(GM_ADDR out, GM_ADDR mOut, GM_ADDR ws,
                                                              TilingData tiling)
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
        rb_d03::LightCube cube;
        cube.Init();

        // ---------- GATE + INTT 握手：Wait(4) → Wait(1) → Cube → Set(3) ----------
        // 背景：S0A 可选 GATE=4；禁 Wait 环内 SyncAll；与 D02 分 launch 复用 flag 号。
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
            // ---------- AIV0：w←INTT(ŵ)；m←extract(v,w)（X12 DataCopy）----------
            rb_d03::ComputeInttAndExtract(ws, mOut);
            TraceMark(traceGm, SLOT_AIV0_DONE, MAGIC_AIV0_DONE);

            // out 魔数：标量指针写（非业务 m；m 已走 DataCopy）
            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3, MAGIC_AIV1_POST_WAIT3);
        }
    }
}
