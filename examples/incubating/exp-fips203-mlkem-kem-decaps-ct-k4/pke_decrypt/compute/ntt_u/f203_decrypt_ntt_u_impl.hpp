/**
 * @file f203_decrypt_ntt_u_impl.hpp
 * @brief Decrypt 流水线：û ← NTT(u') 三段式 MIX（Stage1 split → AIC MMAD → Stage3 pack/merge）。
 *
 * 对齐 FIPS 203 Alg.15 中 NTT(u')；与 f203_decrypt_ntt_u_entry / device_fused 内联段同语义。
 * poly-batch：每 AIV 握完整 poly 的 hi+lo；平面 mat_c；S1–S3 内禁 Gather。
 * golden I/O：中间态 û 不落盘；全链以 m.bin 对拍。
 */
#ifndef F203_DECRYPT_NTT_U_IMPL_HPP
#define F203_DECRYPT_NTT_U_IMPL_HPP

#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_decrypt_ntt_u_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

namespace decrypt_g4 {

/** CrossCore 状态：与 device_fused ST_SPLIT/MMAD/PACK 编号语义对齐。 */
enum NttMachineState : uint16_t {
    NTT_IDLE = 0,
    NTT_AIV_SPLIT,
    NTT_AIC_MMAD,
    NTT_AIV_PACK,
};

/** 等待对端 CrossCore flag（PIPE_MTE2）。 */
__aicore__ inline void ntt_wait(NttMachineState state, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(state);
    KYBER_PIPE_ALL();
}

/** 置位 CrossCore flag，通知对端。 */
__aicore__ inline void ntt_set(NttMachineState state, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(state);
    KYBER_PIPE_ALL();
}

/**
 * NTT polyvec k=4：src 时域 u' → dst NTT 域 û。
 * @param dst/src/ws 输出 / 输入 / workspace（含 LUT）；@param tilingParam mixPass 控制分段
 * 前置：AIC 与双 AIV 均须进入本函数（内部按角色分支）。
 */
__aicore__ inline void ntt_u_impl(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tilingParam)
{
    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tilingParam.tileLength);
    const int32_t mixPass = tilingParam.mixPass;

    const bool runS1 = (mixPass == 0 || mixPass == 3);
    const bool runS2 = (mixPass == 1 || mixPass == 3);
    const bool runS3 = (mixPass == 2 || mixPass == 3);
    const bool syncS1S2 = runS1 && runS2;
    const bool syncS2Pack = runS2;

    NttMachineState state;

    if (AIC) {
        if (!runS2) {
            return;
        }
        if (syncS1S2) {
            state = NTT_AIV_SPLIT;
            ntt_wait(state, AIC, subBlockID);
        }
        state = NTT_AIC_MMAD;
        AicMmad mmad(static_cast<uint16_t>(::tiling::mRowsLogic), coeffN, static_cast<uint16_t>(::tiling::halfN));
        mmad.Init();
        mmad.Process(ws + ::tiling::MAT_C_TMP_LO_EVEN, ws + ::tiling::S0, ws + ::tiling::LUT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + ::tiling::MAT_C_TMP_LO_ODD, ws + ::tiling::S0, ws + ::tiling::LUT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + ::tiling::MAT_C_TMP_HI_EVEN, ws + ::tiling::S0, ws + ::tiling::LUT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(ws + ::tiling::MAT_C_TMP_HI_ODD, ws + ::tiling::S0, ws + ::tiling::LUT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
        if (syncS2Pack) {
            state = NTT_AIV_PACK;
            ntt_set(state, AIC, subBlockID);
        }
    } else {
        if (runS1) {
            state = NTT_AIV_SPLIT;
            AivK8Split split(subBlockID, coeffN);
            split.Init(ws + ::tiling::S0, src);
            split.Process();
            KYBER_PIPE_ALL();
            if (syncS1S2) {
                ntt_set(state, AIC, subBlockID);
            }
        }
        if (runS2) {
            state = NTT_AIV_PACK;
            ntt_wait(state, AIC, subBlockID);
            AivK8PackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(ws + ::tiling::MAT_C_PLANAR, ws + ::tiling::MAT_C_TMP_LO_EVEN, ws + ::tiling::MAT_C_TMP_LO_ODD,
                      ws + ::tiling::MAT_C_TMP_HI_EVEN, ws + ::tiling::MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }
        if (runS3) {
            AivK8RouteAMod merge(subBlockID, coeffN);
            merge.Init(dst, ws + ::tiling::MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
    }
}

} // namespace decrypt_g4

#endif
