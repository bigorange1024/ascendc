/**
 * @file f203_encrypt_intt_e1_kernel.cpp
 * @brief Alg.14 行 19 时域段：u ← INTT(û) + e₁（MIX INTT k=4 + 分片加噪）。
 *
 * 流水线位置：FIPS 203 / ML-KEM-1024 Encrypt 的 **CPU 五 launch 路径** 第 4 核；
 * SIM 生产主路径将本段内联进 `f203_encrypt_l18_l19`（k=8 batch 含 v 行）。
 *
 * 数学：û = Â·ŷ（NTT 域）→ 逆向 NTT → 时域后按半行加 e₁（mod q）。
 * I/O：uNtt GM 输入 [4,256]；e1 GM 输入 [4,256]；uOut GM 输出 [4,256] int32。
 * 与 golden：中间 u 不落盘；CPU 路径随后用 `golden_v` 注入 v 再 pack 出 c。
 *
 * 注意：本核复用 LUT_NTT_* 段装 INTT LUT（CPU phased）；与正向 NTT 核同构，仅源/目的与末尾加噪不同。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_encrypt_at_jp.hpp"
#include "f203_l18_l19_tiling.h"
#include "f203_mod_q/mod_q_add.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

/** CrossCore 握手：与 ntt_y 同构（Split → MMAD → Pack）。 */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1,
    ST_AIC_MMAD = 2,
    ST_AIV_PACK = 3,
};

/** 等待对端 CrossCore flag（PIPE_MTE2 通道 2）。 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/** 向对端置位 CrossCore flag。 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * 设备核：u ← INTT(û) + e₁，KERNEL_TYPE_MIX_AIC_1_2。
 *
 * @param uOut   GM 输出时域 u[4,256] int32（已加 e₁）
 * @param uNtt   GM 输入 NTT 域 û[4,256] int32（at_jp 产物）
 * @param e1     GM 输入噪声 e₁[4,256] int32（prep re 切片）
 * @param ws     workspace：S0、MAT_C_*、INTT LUT（CPU 覆盖写在 LUT_NTT_*）
 * @param tiling tileLength 等
 *
 * AIV 末尾：`mod_q_add_gm_halfrows` 按 subBlock 半行把 e₁ 加到 uOut（避免双 AIV 写冲突）。
 */
extern "C" __global__ __aicore__ void f203_encrypt_intt_e1(GM_ADDR uOut, GM_ADDR uNtt, GM_ADDR e1, GM_ADDR ws,
                                                           TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    FsmState st;

    if (aic) {
        // AIC：等 AIV 编码 û→S0 后，四路 INTT LUT MMAD（偏移名仍为 LUT_NTT_*，内容已换 INTT）
        st = ST_AIV_SPLIT;
        FsmWait(st);
        AicMmad mmad(static_cast<uint16_t>(mRowsLogic), coeffN, static_cast<uint16_t>(halfN));
        mmad.Init();
        mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_NTT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + LUT_NTT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + LUT_NTT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + LUT_NTT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
        st = ST_AIV_PACK;
        FsmSet(st);
    } else {
        // AIV Stage1：从 uNtt GM 读 û，编码到 S0
        st = ST_AIV_SPLIT;
        {
            AivK8Split split(subBlockID, coeffN);
            split.Init(ws + S0, uNtt);
            split.Process();
            KYBER_PIPE_ALL();
        }
        FsmSet(st);

        // Pack + RouteA merge → 时域 u 写 uOut，再半行加 e₁
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
            merge.Init(uOut, ws + MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
        // 半行加噪：AIV0/AIV1 各写自己的 poly 半区，q=3329
        f203_mod_q::mod_q_add_gm_halfrows(uOut, uOut, e1, subBlockID, encrypt_at_jp::kN, encrypt_at_jp::kQ);
        KYBER_PIPE_ALL();
    }
}

#ifndef __CCE_KT_TEST__
/** Host ACL 启动封装。 */
extern "C" void f203_encrypt_intt_e1_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uOut, uint8_t *uNtt,
                                        uint8_t *e1, uint8_t *ws, uint8_t *tiling)
{
    f203_encrypt_intt_e1<<<blockDim, l2ctrl, stream>>>(uOut, uNtt, e1, ws, reinterpret_cast<TilingData *>(tiling));
}
#endif
