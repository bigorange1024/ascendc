/**
 * @file mmad_custom.cpp
 * @brief RB-T10：MIX 真拓扑 — NTT握手(1/3) → MultiplyNTTs → GATE(4) → INTT握手(1/3) → INTT+加噪。
 *
 * 生产 Encrypt 域拼装外形（设备算 u,v；禁 Host 预喂最终 u,v）：
 *   AIC：Wait(1)→CubeNTT→Set(3) → Wait(4) → Wait(1)→CubeINTT→Set(3)
 *   AIV：Set(1)→Wait(3) → Âᵀ∘ŷ / ⟨t̂,ŷ⟩ → Set(4) → Set(1)→Wait(3) → INTT+e/μ
 *
 * 背景：T08 Host 孪生；T09 外形但 Host 预喂 u,v；本刀关 G3 设备侧。
 * 结论：flag 1/3 复用、4=GATE；永禁 5/7；主验收 u,v 对拍。
 * 模式：CrossCore modeId=0x2；Host 单 launch + SynchronizeStream。
 *
 * 若挂死 TRACE 假设：
 *   - 无 POST_WAIT3_NTT → 卡 NTT 握手
 *   - 有 POST_WAIT3_NTT 无 MUL_DONE/PRE_SET4 → 卡 MultiplyNTTs
 *   - 有 SET4 无 INTT → 卡 GATE
 *   - 有 PRE_SET1_INTT 无 POST_WAIT3_INTT → 卡 INTT 复用
 *   - 有 POST_WAIT3_INTT 无 UV_DONE → 卡 INTT+加噪
 */
#include "kernel_operator.h"
#include "light_cube.hpp"
#include "tiling.h"
#include "uv_device_math.hpp"

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
 * @param out   [out] 握手成功魔数（AIV0 写 4B）
 * @param uvOut [out] 设备算出的 u‖v（int32，5120B）；Host 禁预填最终值
 * @param ws    [in/out] 预喂 Â/ŷ/t̂/e/μ/ζ/γ + mat + TRACE；设备写 û/v̂/u/v
 * @param tiling [in] 占位
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR uvOut, GM_ADDR ws,
                                                  TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    using namespace tiling;
    GM_ADDR traceGm = ws + OFF_TRACE;
    GM_ADDR matA = ws + OFF_MAT_A;
    GM_ADDR matB = ws + OFF_MAT_B;
    GM_ADDR matCNtt = ws + OFF_MAT_C_NTT;
    GM_ADDR matCIntt = ws + OFF_MAT_C_INTT;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        // 单 LightCube / 单 TPipe：两段 Process 写不同 mat_c。
        rb_t10::LightCube cube;
        cube.Init();

        // ---------- 段1 NTT 握手：Wait(1) → Cube → Set(3) ----------
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_NTT, MAGIC_AIC_POST_WAIT1_NTT);

        cube.Process(matCNtt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3_NTT, MAGIC_AIC_PRE_SET3_NTT);
        CrossSet(kFlagAicDone);

        // ---------- 段2 GATE：Wait(4) ----------
        CrossWait(kFlagGate);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT4, MAGIC_AIC_POST_WAIT4);

        // ---------- 段3 INTT 握手：复用 Wait(1) → Cube → Set(3) ----------
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_INTT, MAGIC_AIC_POST_WAIT1_INTT);

        cube.Process(matCIntt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3_INTT, MAGIC_AIC_PRE_SET3_INTT);
        CrossSet(kFlagAicDone);
    } else {
        // ---------- AIV 段1 NTT 握手：Set(1) → Wait(3) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_NTT, MAGIC_AIV0_PRE_SET1_NTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_NTT, MAGIC_AIV1_PRE_SET1_NTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_NTT, MAGIC_AIV0_POST_WAIT3_NTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3_NTT, MAGIC_AIV1_POST_WAIT3_NTT);
        }

        // ---------- AIV0：NTT 域 MultiplyNTTs / 内积（真拓扑；非 Host 预喂 u,v）----------
        if (subIdx == 0) {
            rb_t10::ComputeNttDomainUv(ws);
            TraceMark(traceGm, SLOT_AIV0_MUL_DONE, MAGIC_AIV0_MUL_DONE);
        }

        // ---------- AIV 段2 GATE：Set(4) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET4, MAGIC_AIV0_PRE_SET4);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET4, MAGIC_AIV1_PRE_SET4);
        }
        CrossSet(kFlagGate);

        // ---------- AIV 段3 INTT 握手：复用 Set(1) → Wait(3) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_INTT, MAGIC_AIV0_PRE_SET1_INTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_INTT, MAGIC_AIV1_PRE_SET1_INTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_INTT, MAGIC_AIV0_POST_WAIT3_INTT);
            // ---------- AIV0：INTT + e₁ / e₂+μ → u,v ----------
            rb_t10::ComputeInttAddNoise(ws, uvOut);
            TraceMark(traceGm, SLOT_AIV0_UV_DONE, MAGIC_AIV0_UV_DONE);

            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3_INTT, MAGIC_AIV1_POST_WAIT3_INTT);
        }
    }
}
