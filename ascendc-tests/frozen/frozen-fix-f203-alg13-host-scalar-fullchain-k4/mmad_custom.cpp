/**
 * @file mmad_custom.cpp
 * @brief 2s1e MIX kernel 入口（1×AIC Cube + 2×AIV Vector）。
 *
 * ## 流水线（mixPass=0 生产路径）
 *
 *   host src[8,256] + LUT + a_hat
 *     → [AIV] Aiv2s1eSplit        Stage1：limb6 拆分 → workspace S0
 *     → [AIC] AicMmad ×4          Stage2：int8 MMAD（even/odd × lo/hi）
 *     → [AIV] Aiv2s1ePackMatCPlanar  竖堆临时 → 平面 mat_c [96,128]
 *     → [AIV] Aiv2s1eUbPipeline   Stage3 merge/mod + 行18 + 行19–20
 *
 * ## mixPass（tiling.mixPass / CPU 下 g_2s1e_mix_pass）
 *
 *   0 = S1+S2+S3+Hat+Encode（生产）
 *   1 = 仅 S1；2 = 仅 S2；3 = 仅 S3；4 = Hat(+Encode)；5 = S1+S2+S3；7 = 仅 Encode
 *   详见 IMPLEMENTATION_REFERENCE.md §4。
 *
 * ## Host 数据语义
 *
 *   - src 行 0..3：同一 s_poly（FIXED_POLY×4）；行 4..7：e 变体 (e+p mod Q)。
 *   - 设备内复制 ŝ 到双 AIV；**无** SHAT_PEER / AIV↔AIV ŝ GM 交换。
 *   - mat_c 为**平面**布局；NTT S1–S3 **禁止 Gather**。
 *
 * ## 同步
 *
 *   AIC/AIV 通过 CrossCoreSetFlag/WaitFlag 握手：SPLIT → MMAD → PACK →（UB 融合无需额外核间同步）。
 */
#include "2s1e_post_ntt_ub.hpp"
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "integration_config.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"
#if HAT_ALG11_VEC >= 1 && ALG11_MEM_OPS >= 1
/* MIX 核 AIC/AIV 各编一份；ROM 仅 AIV + CPU 链接一次，避免 ld duplicate symbol。 */
#if defined(ASCENDC_CPU_DEBUG) || defined(__DAV_C220_VEC__) || defined(__DAV_C310_VEC__)
#include "alg11_rom_tables.cpp"
#endif
#endif
#if BYTE_ENCODE12_PREFETCH >= 1 && (defined(ASCENDC_CPU_DEBUG) || defined(__DAV_C220_VEC__) || defined(__DAV_C310_VEC__))
#include "byte_encode12_rom_tables.cpp"
#endif

enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT,
    AIC_MMAD,
    AIV_PACK,
    AIV_MERGE,
};

#ifdef ASCENDC_CPU_DEBUG
/** CPU tikicpu：main 写入；内核读取。SIM/设备用 tiling.mixPass。 */
volatile int g_2s1e_mix_pass = 0;
#endif

__aicore__ inline void __WAIT(MachineState STATE, const bool AIC, const int32_t subBlockID)
{
    (void)AIC;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(STATE);
    KYBER_PIPE_ALL();
}

__aicore__ inline void __SET(MachineState STATE, const bool AIC, const int32_t subBlockID)
{
    (void)AIC;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(STATE);
    KYBER_PIPE_ALL();
}

#define WAIT __WAIT(STATE, AIC, subBlockID);
#define SET  __SET(STATE, AIC, subBlockID);

extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR dst, GM_ADDR t_hat, GM_ADDR ek_out, GM_ADDR sk_out,
                                                  GM_ADDR src, GM_ADDR a_hat, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    const auto halfCols = static_cast<uint16_t>(halfN);

#ifdef ASCENDC_CPU_DEBUG
    const int32_t mixPass = g_2s1e_mix_pass;
#else
    const int32_t mixPass = tiling.mixPass;
#endif
    /* --- mixPass 阶段开关（见 IMPLEMENTATION_REFERENCE.md §4）--- */
    const bool runS1 = (mixPass == 0 || mixPass == 1 || mixPass == 5);
    const bool runS2 = (mixPass == 0 || mixPass == 2 || mixPass == 5);
    const bool runS3Only = (mixPass == 5);
    const bool runS3 = (mixPass == 0 || mixPass == 3 || mixPass == 5);
    const bool runHat = (mixPass == 0 || mixPass == 4);
#if HAT_BYTE_ENCODE >= 1 && HAT_LINE18_DOT_ONLY < 1
    const bool runEncode = (mixPass == 0 || mixPass == 4 || mixPass == 7);
#else
    const bool runEncode = false;
#endif
    const bool ubFused = runHat || (runEncode && mixPass == 7) || (runS3 && !runS3Only);
    const bool loadNttPreset = (mixPass == 4 || mixPass == 7);  /* 跳过 S3，dst 作 ŝ/ê 预设 */
    const bool loadThatPreset = (mixPass == 7);                   /* 跳过行 18，t_hat 作预设 */
    const bool syncS1S2 = runS1 && runS2;
    const bool syncS2Pack = runS2;

    MachineState STATE;

    /* ========== AIC：Stage2 int8 MMAD ×4（even/odd × lo/hi）========== */
    if (AIC) {
        if (!runS2) {
            return;
        }
        if (syncS1S2) {
            STATE = AIV_SPLIT;
            WAIT
        }
        STATE = AIC_MMAD;
        AicMmad mmad(static_cast<uint16_t>(mRows), coeffN, halfCols);
        mmad.Init();
        mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + LUT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + LUT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + LUT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
        if (syncS2Pack) {
            STATE = AIV_PACK;
            SET
        }
    } else {
        /* ========== AIV：Stage1 拆分 ========== */
        if (runS1) {
            STATE = AIV_SPLIT;
            Aiv2s1eSplit split(subBlockID, coeffN);
            split.Init(ws + S0, src);
            split.Process();
            KYBER_PIPE_ALL();
            if (syncS1S2) {
                SET
            }
        }

        /* ========== AIV：Stage2 后平面 pack ========== */
        if (runS2) {
            STATE = AIV_PACK;
            WAIT
            Aiv2s1ePackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                      ws + MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }

        /* ========== AIV：S3 + 行18 + 行19–20（单 TPipe UB 融合）========== */
        if (ubFused || runS3) {
            Aiv2s1eUbPipeline pipe(subBlockID, coeffN);
            pipe.Init(ws + MAT_C_PLANAR, a_hat, ek_out, sk_out, dst, t_hat);
            pipe.Process(runS3 && !loadNttPreset, runHat, runEncode, loadNttPreset, loadThatPreset);
            KYBER_PIPE_ALL();
        }
    }
}
