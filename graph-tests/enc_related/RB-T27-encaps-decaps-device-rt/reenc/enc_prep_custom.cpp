/**
 * @file prep_custom.cpp
 * @brief RB-T27 Encaps/Reenc Launch：prep（AIV-only）— ek/ρ + 设备 G/H。
 *
 * 相对 T26：ek/ρ 镜像与 h/K/coins 写出均走 UB+DataCopy（X12；禁 SetValue 写 GM）。
 * m 由 Host 预装 OFF_M；blockDim=1；无 CrossCore。
 */
#include "reenc/encaps_g_device.hpp"
#include "kernel_operator.h"
#include "reenc/tiling.h"

/** TRACE / mark：uint32 标量写（非业务密文/密钥写出路径）。 */
__aicore__ inline void MarkU32(GM_ADDR base, uint32_t slotOrZero, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(base);
    *(p + slotOrZero) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * prep：完整 ek / ρ + 设备 Encaps 头 G/H → K‖coins。
 * @param ekIn [in]  ek_PKE[1568]
 * @param ws   [in/out] 读 OFF_M；写 OFF_EK/OFF_RHO/OFF_COINS/OFF_K/OFF_H/TRACE
 * @param tiling 占位
 */
extern "C" __global__ __aicore__ void enc_prep_custom(GM_ADDR ekIn, GM_ADDR ws, EncTilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    using namespace enc_tiling;

    AscendC::GlobalTensor<uint8_t> gmEkIn;
    AscendC::GlobalTensor<uint8_t> gmEkWs;
    AscendC::GlobalTensor<uint8_t> gmRho;

    gmEkIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ekIn),
                           static_cast<uint32_t>(kEkBytes));
    gmEkWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_EK),
                           static_cast<uint32_t>(kEkBytes));
    gmRho.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_RHO),
                          static_cast<uint32_t>(kRhoBytes));

    // ---- X12：ek → OFF_EK（GM←UB←GM；禁 SetValue 写 GM）----
    {
        AscendC::TPipe pipe;
        AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
        AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
        pipe.InitBuffer(queIn, 1, static_cast<uint32_t>(kEkBytes));
        pipe.InitBuffer(queOut, 1, static_cast<uint32_t>(kEkBytes));
        AscendC::LocalTensor<uint8_t> inLoc = queIn.AllocTensor<uint8_t>();
        AscendC::LocalTensor<uint8_t> outLoc = queOut.AllocTensor<uint8_t>();
        AscendC::DataCopy(inLoc, gmEkIn, static_cast<uint32_t>(kEkBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        for (uint32_t i = 0; i < static_cast<uint32_t>(kEkBytes); ++i) {
            outLoc.SetValue(i, inLoc.GetValue(i));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmEkWs, outLoc, static_cast<uint32_t>(kEkBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        // ρ ← ek 尾 32B
        for (uint32_t i = 0; i < static_cast<uint32_t>(kRhoBytes); ++i) {
            outLoc.SetValue(i, inLoc.GetValue(static_cast<uint32_t>(kEkBytes - kRhoBytes) + i));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmRho, outLoc, static_cast<uint32_t>(kRhoBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        queIn.FreeTensor(inLoc);
        queOut.FreeTensor(outLoc);
    }

    // ---- 设备 Encaps 头：h/K/coins 亦走 DataCopy（encaps_g_device）----
    EncapsGDevice::RunEncapsHeadGFromWs(ws);
    MarkU32(ws + OFF_TRACE, SLOT_AIV0_G_DONE, MAGIC_AIV0_G_DONE);

    MarkU32(ws + OFF_PREP_MARK, 0, MAGIC_PREP_MARK);
    MarkU32(ws + OFF_TRACE, SLOT_PREP_DONE, MAGIC_PREP_DONE);
}
