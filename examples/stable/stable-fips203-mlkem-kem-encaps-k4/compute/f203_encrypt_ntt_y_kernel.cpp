/**
 * @file f203_encrypt_ntt_y_kernel.cpp
 * @brief Alg.14 行 16–17 单段：ŷ ← NTT(y)，MIX k=4（与 Tag5T stage123 同构，独立 launch）。
 *
 * 流水线位置：FIPS 203 / ML-KEM-1024 Encrypt 的 **CPU 五 launch 路径** 第 2 核；
 * SIM 生产主路径将本段内联进 `f203_encrypt_l18_l19`，本文件仅 CPU/对照用。
 *
 * 数学：y = r（prep 输出 re 前 4 poly）→ 正向 NTT → ŷ[4,256] int32。
 * I/O：ySrc GM 输入 [4,256] int32；yHat GM 输出 [4,256] int32；ws 含 S0/mat_c/LUT_NTT_*。
 * 与 golden：中间态不落盘；最终密文对拍仍以 `golden/c.bin` 为准。
 *
 * FSM（AIC↔AIV CrossCore flag）：
 *   ST_AIV_SPLIT(1)：AIV 完成 Stage1 limb 编码写 S0 → AIC 四路 MMAD
 *   ST_AIV_PACK(3)：AIC 写完 mat_c 临时 → AIV Pack+RouteA merge 写 yHat
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_l18_l19_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

/** CrossCore 握手状态：与 l18_l19 内 NTT 子段同构，独立 launch 时仅用 1/2/3。 */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1,  // AIV Stage1 完成，AIC 可开 MMAD
    ST_AIC_MMAD = 2,   // 保留编号（本核未单独 Wait 此态）
    ST_AIV_PACK = 3,   // AIC MMAD 完成，AIV 可 Pack/merge
};

#ifdef ASCENDC_CPU_DEBUG
/** CPU 孪生：host 注入 mixPass（与 tiling.mixPass 对齐）；默认 3=全量 S1–S3。 */
volatile int g_f203_ntt_y_mix_pass = 3;
#endif

/**
 * 等待对端 CrossCore 置位指定 FSM 状态。
 * @param st 期望的 flag 值（PIPE_MTE2 通道 2）
 * 前置：调用前后均 PipeBarrier，避免与搬运/计算乱序。
 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * 向对端广播当前 FSM 状态已完成。
 * @param st 要置位的 flag
 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * 设备核：ŷ ← NTT(y)，KERNEL_TYPE_MIX_AIC_1_2（1 AIC + 2 AIV）。
 *
 * @param yHat   GM 输出 ŷ[4,256] int32（行优先）
 * @param ySrc   GM 输入 y=r[4,256] int32（prep re 切片）
 * @param ws     workspace：S0、MAT_C_*、LUT_NTT_*（host 已装 LUT）
 * @param tiling tileLength 等；CPU 下 mixPass 经 g_f203_ntt_y_mix_pass 注入
 *
 * 分支：
 *   AIC：等 AIV Split → 四路 AicMmad（lo/hi × even/odd LUT）→ 通知 Pack
 *   AIV：Stage1 编码 → 通知 AIC；等 Pack → 平面 mat_c 拼装 → RouteA merge 写 yHat
 */
extern "C" __global__ __aicore__ void f203_encrypt_ntt_y(GM_ADDR yHat, GM_ADDR ySrc, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    // subBlockNum==1 判定 AIC；否则为 AIV，subBlockIdx 区分 AIV0/AIV1（各握 2 poly）
    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);  // 通常 256
    FsmState st;

    if (aic) {
        // ---- AIC：等 AIV 写完 S0，再对 even/odd×lo/hi 四路 LUT 做 MMAD ----
        st = ST_AIV_SPLIT;
        FsmWait(st);
        AicMmad mmad(static_cast<uint16_t>(mRowsLogic), coeffN, static_cast<uint16_t>(halfN));
        mmad.Init();
        // 四路：MAT_C_TMP_{LO,HI}_{EVEN,ODD} ← S0 × LUT_NTT_*（planar-stacked top/bottom）
        mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_NTT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + LUT_NTT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + LUT_NTT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + LUT_NTT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
        st = ST_AIV_PACK;
        FsmSet(st);  // 通知 AIV 可开始 Pack+merge
    } else {
        // ---- AIV：Stage1 limb 编码 y → S0[16,256] int8 ----
        {
            st = ST_AIV_SPLIT;
            AivK8Split split(subBlockID, coeffN);
            split.Init(ws + S0, ySrc);
            split.Process();
            KYBER_PIPE_ALL();
            FsmSet(st);  // 通知 AIC 可 MMAD
        }

        // ---- AIV：等 AIC → 四路临时拼平面 mat_c → RouteA 合并写 ŷ ----
        st = ST_AIV_PACK;
        FsmWait(st);
        {
            AivK8PackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                      ws + MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }
        {
            AivK8RouteAMod merge(subBlockID, coeffN);
            merge.Init(yHat, ws + MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
    }
}

#ifndef __CCE_KT_TEST__
/** Host 侧 ACL 启动封装：blockDim/stream 由 runtime 传入，tiling 指针转 TilingData。 */
extern "C" void f203_encrypt_ntt_y_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *yHat, uint8_t *ySrc,
                                      uint8_t *ws, uint8_t *tiling)
{
    f203_encrypt_ntt_y<<<blockDim, l2ctrl, stream>>>(yHat, ySrc, ws, reinterpret_cast<TilingData *>(tiling));
}
#endif
