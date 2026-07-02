/**
 * @file f203_decrypt_ntt_u_impl.hpp
 * @brief NTT(u) 三段式 MIX 段（g4_full 内联；与 f203_decrypt_ntt_u_entry 同语义）。
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

enum NttMachineState : uint16_t {
    NTT_IDLE = 0,
    NTT_AIV_SPLIT,
    NTT_AIC_MMAD,
    NTT_AIV_PACK,
};

__aicore__ inline void ntt_wait(NttMachineState state, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(state);
    KYBER_PIPE_ALL();
}

__aicore__ inline void ntt_set(NttMachineState state, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(state);
    KYBER_PIPE_ALL();
}

/** NTT polyvec k=4：src 时域 u → dst NTT 域 u_hat。AIC/AIV 均须进入。 */
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
