// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/mmad_custom.cpp
// @layer compute
// @role mmad_custom 设备主核：AIC MMAD + AIV NTT/Alg11/行18–20。 / Primary device kernel.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: 2s1e_post_ntt_ub.hpp, aic_func.hpp, aiv_func.hpp, basic.hpp, integration_config.hpp, kernel_operator.h, kyber_limb6.hpp, tiling.h, alg11_rom_tables.cpp, byte_encode12_rom_tables.cpp, f203_keygen_ek_pke_fuse.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file mmad_custom.cpp
 * @brief KeyGen Launch 2：2s1e MIX 主核（1×AIC Cube + 2×AIV Vector）。
 *
 * ## 流水线位置
 * FIPS 203 Alg.13 行 16–21（NTT(ŝ/ê) → t̂=Â∘ŝ+ê → ByteEncode → ek‖ρ）。
 * 读 prep 写出的 src/a_hat/ρ + Host LUT；写 ek_pke / dk_pke（及中间 dst/t_hat）。
 *
 * ## 对齐与 golden
 * ML-KEM-1024（k=4）；验收仅 I/O 等价于 `keygen_golden.py`，不要求与 Host 源码同构。
 *
 * ## 流水线（mixPass=0 生产路径）
 *
 *   host src[8,256] + LUT + a_hat
 *     → [AIV] Aiv2s1eSplit        Stage1：limb6 拆分 → workspace S0
 *     → [AIC] AicMmad ×4          Stage2：int8 MMAD（even/odd × lo/hi）
 *     → [AIV] Aiv2s1ePackMatCPlanar  竖堆临时 → 平面 mat_c [96,128]
 *     → [AIV] Aiv2s1eUbPipeline   Stage3 merge/mod + 行18 + 行19–20
 *     → [AIV0] FuseEkPke          行 21 ek_polyvec ‖ ρ（F203_KEYGEN_EK_PKE=1）
 *
 * ## mixPass（tiling.mixPass / CPU 下 g_2s1e_mix_pass）
 *
 *   0 = S1+S2+S3+Hat+Encode（生产）
 *   1 = 仅 S1；2 = 仅 S2；3 = 仅 S3；4 = Hat(+Encode)；5 = S1+S2+S3；6 = AicMmad sanity；7 = 仅 Encode
 *   详见 IMPLEMENTATION_REFERENCE.md §4。调试分段须显式覆盖，默认必须为 0。
 *
 * ## Host 数据语义
 *
 *   - src 行 0..3：ŝ；行 4..7：ê（prep CBD 输出）。
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

/** 跨核握手状态机：AIV_SPLIT→AIC_MMAD→AIV_PACK（AIV_MERGE 历史保留） */
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

/**
 * 等待对端 CrossCore 标志（先 PIPE_ALL，再 WaitFlag）。
 * @param STATE 期望状态枚举值（作 flag id）
 * @param AIC / subBlockID 保留参数（宏接口统一签名）
 */
__aicore__ inline void __WAIT(MachineState STATE, const bool AIC, const int32_t subBlockID)
{
    (void)AIC;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(STATE);
    KYBER_PIPE_ALL();
}

/**
 * 向对端置 CrossCore 标志（先 PIPE_ALL，再 SetFlag）。
 * 语义对称于 __WAIT；AIC 完成 MMAD 后 SET(AIV_PACK)，AIV 完成 Split 后 SET(AIV_SPLIT)。
 */
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

#if F203_KEYGEN_EK_PKE >= 1
#include "f203_keygen_ek_pke_fuse.hpp"
#endif

/**
 * MIX 主核：Tag5T NTT + Alg.11 + ByteEncode（+ 可选行 21 融合）。
 * @param dst     输出/中间：NTT 后 ŝ/ê [8,256] int32
 * @param t_hat   输出/中间：t̂ [4,256] int32
 * @param ek_out  输出：ByteEncode₁₂(t̂) 1536B（亦作 Fuse 输入）
 * @param sk_out  输出：ByteEncode₁₂(ŝ) 即 dk_pke 1536B
 * @param src     输入：prep 的 ŝ‖ê [8,256] int32
 * @param a_hat   输入：Â[16,256] int32
 * @param ws      workspace：S0 / mat_c 临时 / 平面 mat_c / LUT
 * @param tiling  Host TilingData（tileLength、mixPass）
 * @param rho_gm / ek_pke_gm  仅 F203_KEYGEN_EK_PKE：ρ 与最终 ek_PKE
 * 前置：KERNEL_TYPE_MIX_AIC_1_2；生产 mixPass=0。
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR dst, GM_ADDR t_hat, GM_ADDR ek_out, GM_ADDR sk_out,
                                                  GM_ADDR src, GM_ADDR a_hat, GM_ADDR ws, TilingData tiling
#if F203_KEYGEN_EK_PKE >= 1
                                                  ,
                                                  GM_ADDR rho_gm, GM_ADDR ek_pke_gm
#endif
                                                  )
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    // GetSubBlockNum()==1 表示本核是 AIC；否则为 AIV，subBlockID∈{0,1}
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
    // ubFused：S3/Hat/Encode 走同一 Aiv2s1eUbPipeline，避免多次 TPipe
    const bool ubFused = runHat || (runEncode && mixPass == 7) || (runS3 && !runS3Only);
    const bool loadNttPreset = (mixPass == 4 || mixPass == 7);  /* 跳过 S3，dst 作 ŝ/ê 预设 */
    const bool loadThatPreset = (mixPass == 7);                   /* 跳过行 18，t_hat 作预设 */
    const bool syncS1S2 = runS1 && runS2;
    const bool syncS2Pack = runS2;

    MachineState STATE;

    /* ========== mixPass=6：AicMmad sanity（tag5t 同维 16×256×128，单路 lo_even）========== */
    if (mixPass == 6) {
        if (AIC) {
            STATE = AIC_MMAD;
            AicMmad mmad(static_cast<uint16_t>(sanityMRows), coeffN, halfCols);
            mmad.Init();
            mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_EVEN_TOP);
            KYBER_PIPE_ALL();
            STATE = AIV_PACK;
            SET
        } else {
            STATE = AIV_PACK;
            WAIT
        }
        return;
    }

    /* ========== AIC：Stage2 int8 MMAD ×4（even/odd × lo/hi）========== */
    if (AIC) {
        if (!runS2) {
            return;
        }
        // 等 AIV 写完 S0（limb6 int8）后再开 Cube
        if (syncS1S2) {
            STATE = AIV_SPLIT;
            WAIT
        }
        STATE = AIC_MMAD;
        /** MMAD 仅消费 S0 有效行 mRowsLogic(32)；mRows 尾部 8 行仅为 ws 对齐垫片 */
        AicMmad mmad(static_cast<uint16_t>(mRowsLogic), coeffN, halfCols);
        mmad.Init();
        // 四路：lo×even、lo×odd、hi×even、hi×odd → 四个竖堆临时区
        mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + LUT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + LUT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + LUT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
#ifdef ASCENDC_CPU_DEBUG
        AscendC::PipeBarrier<PIPE_ALL>();
#endif
        // 通知 AIV 可做平面 pack
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

#if F203_KEYGEN_EK_PKE >= 1
    /* KeyGen：行 21 ek_PKE = ek_polyvec ‖ ρ，与行 16–20 同次 Launch；仅 AIV0 */
    if (!AIC && subBlockID == 0 && runEncode) {
        F203KeygenEkPke::FuseEkPke(ek_out, rho_gm, ek_pke_gm);
        KYBER_PIPE_ALL();
    }
#endif
}

#ifndef __CCE_KT_TEST__
#if F203_KEYGEN_EK_PKE >= 1
extern "C" void mmad_custom_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *dst, uint8_t *t_hat,
                               uint8_t *ek_out, uint8_t *sk_out, uint8_t *src, uint8_t *a_hat, uint8_t *ws,
                               uint8_t *tiling, uint8_t *rho_gm, uint8_t *ek_pke_gm)
{
    mmad_custom<<<blockDim, l2ctrl, stream>>>(dst, t_hat, ek_out, sk_out, src, a_hat, ws,
                                              *reinterpret_cast<TilingData *>(tiling), rho_gm, ek_pke_gm);
}
#endif
#endif
