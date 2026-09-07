/**
 * @file shake_l1_ub.hpp
 * @brief E05 L1：真 SHAKE256 短向量积木（全 UB + 可选写 GM）。
 *
 * 流水线位置：被 mmad_custom L1 phase 调用；积木来自本目录 vendor/ 自包含拷贝
 * （shake_xof_kernel + keccak_f1600_kernel），语义对齐 pass-shake256-ascendc-toy。
 *
 * 用例：消息 "abc"（3B）→ SHAKE256 输出 32B；设备侧 UB 对拍内嵌 golden，
 * 并将 y 写到 GM 供 Host 再验。
 *
 * 背景：图谱 D-exp-e05 — E04 壳上把 L1 假采样换成真 SHAKE；保留 L2 真 NTT+SET(4)。
 * 未采用：抄 Encrypt；改 shared 原文件；GM 大 batch 独立 launch。
 */
#pragma once

#include "shake_general.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"

#include "kernel_operator.h"

namespace ShakeL1Toy {

/** 短向量：msg="abc"，outLen=32（与 pass-shake256-ascendc-toy 的 abc 用例同形）。 */
constexpr uint32_t kBatch = 1U;
constexpr uint32_t kMaxMsgLen = 3U;
constexpr uint32_t kOutLen = 32U;
constexpr uint32_t kXBytes = 3U;
constexpr uint32_t kYBytes = 32U;

/** 消息常量：'a''b''c' */
constexpr uint8_t kX[kXBytes] = {0x61U, 0x62U, 0x63U};

/**
 * hashlib.shake_256(b"abc").digest(32) 小端字节（Host gen_data / Python 同源）。
 * 仅作 I/O oracle，非设备实现规格。
 */
constexpr uint8_t kGoldenY[kYBytes] = {
    0x48U, 0x33U, 0x66U, 0x60U, 0x13U, 0x60U, 0xa8U, 0x77U, 0x1cU, 0x68U, 0x63U, 0x08U,
    0x0cU, 0xc4U, 0x11U, 0x4dU, 0x8dU, 0xb4U, 0x45U, 0x30U, 0xf8U, 0xf1U, 0xe1U, 0xeeU,
    0x4fU, 0x94U, 0xeaU, 0x37U, 0xe7U, 0x8bU, 0x57U, 0x39U,
};

/**
 * 填入消息与长度到 UB。
 * @param xUb   [out] 消息缓冲 [batch,maxMsgLen]
 * @param lenUb [out] 长度缓冲 [batch]
 */
__aicore__ inline void FillAbcUb(AscendC::LocalTensor<uint8_t> &xUb, AscendC::LocalTensor<uint32_t> &lenUb)
{
    for (uint32_t i = 0; i < kXBytes; ++i) {
        xUb.SetValue(i, kX[i]);
    }
    lenUb.SetValue(0, kXBytes);
}

/**
 * UB 输出与内嵌 golden 逐字节比对。
 * @return 1=PASS，0=FAIL
 */
__aicore__ inline uint32_t CompareYUbToGolden(const AscendC::LocalTensor<uint8_t> &yUb)
{
    for (uint32_t i = 0; i < kYBytes; ++i) {
        if (yUb.GetValue(i) != kGoldenY[i]) {
            return 0U;
        }
    }
    return 1U;
}

/**
 * 跑短向量 SHAKE256：UB 填 "abc" → ProcessInline → UB 对拍 → 可选写 GM。
 * @param yGmOut [out] 可为 nullptr；非空则把 32B y 写到 GM（供 Host 对拍）
 * @return 1=设备 UB 对拍 PASS，0=FAIL
 * 前置：仅 AIV 调用；内嵌 ProcessInline（不受外层 subBlock 影响）。
 */
__aicore__ inline uint32_t RunShake256AbcUb(__gm__ uint8_t *yGmOut)
{
    ShakeGeneralTilingData tilingHost{};
    ShakeXofUb::FillShakeTilingUb(tilingHost, kBatch, kMaxMsgLen, kOutLen, SHAKE256_RATE_BYTES);
    tilingHost.blockDim = 1U;

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> lenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> stagingBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> yBuf;
    pipe.InitBuffer(xBuf, ShakeXofUb::CeilAlign32(kXBytes));
    pipe.InitBuffer(lenBuf, ShakeXofUb::CeilAlign32(kBatch * static_cast<uint32_t>(sizeof(uint32_t))));
    pipe.InitBuffer(stagingBuf, ShakeXofKernel::SHAKE_XOF_STAGING_BYTES);
    pipe.InitBuffer(yBuf, ShakeXofUb::CeilAlign32(kYBytes));

    AscendC::LocalTensor<uint8_t> xUb = xBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lenUb = lenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> stagingUb = stagingBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> yUb = yBuf.Get<uint8_t>();

    FillAbcUb(xUb, lenUb);
    ShakeXofUb::PipeAll();

    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lenUb, yUb, stagingUb, &tilingHost);
    ShakeXofUb::PipeAll();

    const uint32_t pass = CompareYUbToGolden(yUb);

    // 写 GM：32B 对齐拷贝，供 Host 再验（L2 会覆盖 out 的 NTT 区，故 Host 须在 L1 Sync 后立刻 D2H）
    if (yGmOut != nullptr) {
        AscendC::GlobalTensor<uint8_t> yGm;
        yGm.SetGlobalBuffer(yGmOut);
        AscendC::DataCopy(yGm, yUb, ShakeXofUb::CeilAlign32(kYBytes));
        ShakeXofUb::PipeAll();
    }
    return pass;
}

} // namespace ShakeL1Toy
