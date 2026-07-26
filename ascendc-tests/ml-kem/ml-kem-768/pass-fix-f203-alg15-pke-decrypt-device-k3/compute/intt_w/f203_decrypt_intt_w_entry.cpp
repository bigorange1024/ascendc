/**
 * @file f203_decrypt_intt_w_entry.cpp
 * @brief Decrypt 流水线独立入口：w ← INTT(ŵ_padded) 三段式 MIX（mixPass=3）。
 *
 * 对齐 FIPS 203 Alg.15 中 INTT(ŵ)；与 NTT 同 Stage1–3 骨架，LUT 为逆变换表。
 * 输入 src [k,256] int32（NTT 域，已 pad）；输出 dst [k,256] int32（时域）。
 * 探针：pass-fix-f203-alg15-pke-decrypt-device-k3（compute/intt_w）。
 * 生产路径内联于 f203_decrypt_device_fused；golden 以全链 m.bin 对拍。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_decrypt_intt_w_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

/** MIX 跨核握手状态：S1 split → S2 MMAD → S2 pack（再进 S3 merge）。 */
enum InttMachineState : uint16_t {
    INTT_IDLE = 0,
    INTT_AIV_SPLIT,
    INTT_AIC_MMAD,
    INTT_AIV_PACK,
};

#ifdef ASCENDC_CPU_DEBUG
/** CPU tikicpu：main 可改写；SIM/设备用 tiling.mixPass（生产默认 3=全段）。 */
volatile int g_f203_decrypt_intt_w_mix_pass = 3;
#endif

/** AIV/AIC 等待对端 CrossCore 标志（PIPE_MTE2 域）。 */
__aicore__ inline void intt_wait(InttMachineState state, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(state);
    KYBER_PIPE_ALL();
}

/** 置位 CrossCore 标志，通知对端可进入下一阶段。 */
__aicore__ inline void intt_set(InttMachineState state, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(state);
    KYBER_PIPE_ALL();
}

#define INTT_WAIT intt_wait(state, AIC, subBlockID);
#define INTT_SET intt_set(state, AIC, subBlockID);

/**
 * INTT(ŵ_padded) 独立入口：1×AIC + 2×AIV。
 * @param dst 时域输出 [k,256] int32
 * @param src NTT 域输入（已 pad）[k,256] int32
 * @param ws 含 S0 / mat_c 临时 / even-odd LUT 的 workspace
 * @param tiling tileLength、mixPass 等
 */
extern "C" __global__ __aicore__ void f203_decrypt_intt_w(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);

#ifdef ASCENDC_CPU_DEBUG
    const int32_t mixPass = g_f203_decrypt_intt_w_mix_pass;
#else
    const int32_t mixPass = tiling.mixPass;
#endif

    // mixPass：0=仅S1，1=仅S2，2=仅S3，3=全链路（生产）
    const bool runS1 = (mixPass == 0 || mixPass == 3);
    const bool runS2 = (mixPass == 1 || mixPass == 3);
    const bool runS3 = (mixPass == 2 || mixPass == 3);
    const bool syncS1S2 = runS1 && runS2;
    const bool syncS2Pack = runS2;

    InttMachineState state;

    if (AIC) {
        // --- AIC：仅 Stage2 四路 MMAD（逆变换 LUT）---
        if (!runS2) {
            return;
        }
        if (syncS1S2) {
            state = INTT_AIV_SPLIT;
            INTT_WAIT  // 等 AIV 写完 S0
        }
        state = INTT_AIC_MMAD;
        AicMmad mmad(static_cast<uint16_t>(mRowsLogic), coeffN, static_cast<uint16_t>(halfN));
        mmad.Init();
        // lo/hi × even/odd 各乘对应 LUT 半区
        mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + LUT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + LUT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + LUT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
        if (syncS2Pack) {
            state = INTT_AIV_PACK;
            INTT_SET  // 通知 AIV 可 pack
        }
    } else {
        // --- AIV：S1 split →（等 AIC）S2 pack → S3 RouteA merge ---
        if (runS1) {
            state = INTT_AIV_SPLIT;
            AivK8Split split(subBlockID, coeffN);
            split.Init(ws + S0, src);
            split.Process();
            KYBER_PIPE_ALL();
            if (syncS1S2) {
                INTT_SET
            }
        }
        if (runS2) {
            state = INTT_AIV_PACK;
            INTT_WAIT
            AivK8PackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                      ws + MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }
        if (runS3) {
            AivK8RouteAMod merge(subBlockID, coeffN);
            merge.Init(dst, ws + MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
    }
}
