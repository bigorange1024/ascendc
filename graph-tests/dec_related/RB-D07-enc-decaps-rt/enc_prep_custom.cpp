/**
 * @file enc_prep_custom.cpp
 * @brief RB-D07 Encaps/ReEnc 共用 Launch1：AIV-only — coins/完整 ek → workspace；ρ←ek 尾。
 *
 * 本文件在流水线中的位置：Encaps 与 Decaps Re-Encrypt 的 prep（无 CrossCore）。
 * Host mid-sync 后再启 enc_mix_custom（MIX Encrypt 面）。basename=enc_prep_custom。
 *
 * 背景：S0B / Encrypt KB §B2 — 双 launch 拆 CrossCore 面；X12 业务 GM 禁 SetValue。
 * 结论：KERNEL_TYPE_AIV_ONLY；blockDim=1；coins/ek/ρ 均 UB+DataCopy 写出。
 * 未采用：抄 T25–T27；与 Decrypt fused；flag 5/7；Host 预喂最终 c。
 */
#include "kernel_operator.h"
#include "tiling.h"

/** 向 TRACE / mark GM 写 uint32 魔数（标量 `__gm__` 写，兼容 CAModel；非业务密文）。 */
__aicore__ inline void MarkU32(GM_ADDR base, uint32_t slotOrZero, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(base);
    *(p + slotOrZero) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * Launch1 prep：r'/coins + 完整 ek + ρ 写入共享 workspace（X12 DataCopy）。
 * @param coinsIn [in]  Host r'[32]（K01 G 语义；本刀独立喂 bin）
 * @param ekIn    [in]  ek_PKE[1568]=BE₁₂(t̂)‖ρ；镜像到 OFF_EK，尾 32B 为 ρ
 * @param ws      [in/out] 写 OFF_COINS / OFF_EK / OFF_RHO / OFF_PREP_MARK / TRACE
 * @param tiling  占位
 * 前置：blockDim=1；AIV-only；无 CrossCore。
 */
extern "C" __global__ __aicore__ void enc_prep_custom(GM_ADDR coinsIn, GM_ADDR ekIn, GM_ADDR ws,
                                                        TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    using namespace tiling;

    AscendC::GlobalTensor<uint8_t> gmCoinsIn;
    AscendC::GlobalTensor<uint8_t> gmCoinsWs;
    AscendC::GlobalTensor<uint8_t> gmEkIn;
    AscendC::GlobalTensor<uint8_t> gmEkWs;
    AscendC::GlobalTensor<uint8_t> gmRho;

    gmCoinsIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(coinsIn),
                              static_cast<uint32_t>(kCoinsBytes));
    gmCoinsWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_COINS),
                              static_cast<uint32_t>(kCoinsBytes));
    gmEkIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ekIn),
                           static_cast<uint32_t>(kEkBytes));
    gmEkWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_EK),
                           static_cast<uint32_t>(kEkBytes));
    gmRho.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_RHO),
                          static_cast<uint32_t>(kRhoBytes));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    // ek=1568B 为最大业务块；coins/ρ 复用同一对 UB 槽
    pipe.InitBuffer(queIn, 1, static_cast<uint32_t>(kEkBytes));
    pipe.InitBuffer(queOut, 1, static_cast<uint32_t>(kEkBytes));

    AscendC::LocalTensor<uint8_t> inLoc = queIn.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> outLoc = queOut.AllocTensor<uint8_t>();

    // ---- X12：coins(r') → OFF_COINS（GM←UB←GM；禁 SetValue 写业务 GM）----
    AscendC::DataCopy(inLoc, gmCoinsIn, static_cast<uint32_t>(kCoinsBytes));
    AscendC::PipeBarrier<PIPE_ALL>();
    for (uint32_t i = 0; i < static_cast<uint32_t>(kCoinsBytes); ++i) {
        outLoc.SetValue(i, inLoc.GetValue(i));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmCoinsWs, outLoc, static_cast<uint32_t>(kCoinsBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- X12：完整 ek → OFF_EK（Launch2 Decode₁₂ 读体）----
    AscendC::DataCopy(inLoc, gmEkIn, static_cast<uint32_t>(kEkBytes));
    AscendC::PipeBarrier<PIPE_ALL>();
    for (uint32_t i = 0; i < static_cast<uint32_t>(kEkBytes); ++i) {
        outLoc.SetValue(i, inLoc.GetValue(i));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmEkWs, outLoc, static_cast<uint32_t>(kEkBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- X12：ρ ← ek 尾 32B ----
    for (uint32_t i = 0; i < static_cast<uint32_t>(kRhoBytes); ++i) {
        outLoc.SetValue(i, inLoc.GetValue(static_cast<uint32_t>(kEkBytes - kRhoBytes) + i));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmRho, outLoc, static_cast<uint32_t>(kRhoBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    queIn.FreeTensor(inLoc);
    queOut.FreeTensor(outLoc);

    MarkU32(ws + OFF_PREP_MARK, 0, MAGIC_PREP_MARK);
    MarkU32(ws + OFF_TRACE, SLOT_PREP_DONE, MAGIC_PREP_DONE);
}
