/**
 * @file enc_g_custom.cpp
 * @brief RB-D07 / DRW-K04：Encaps 与 Decaps **共用** AIV G 核 — G(m‖h)→(K‖r)。
 *
 * 本文件在流水线中的位置：
 *   - Encaps：m=消息，h=dk[3104:3136) → (K_enc, r)；coins=r 喂后续 enc_prep/mix。
 *   - Decaps：m'=Decrypt 终产物，同一 h → (K', r')；供 ReEnc + FO。
 * 契约继承 D04-decaps-G / T22 思路（设备 G、Host 不预喂 coins）；basename=enc_g_custom。
 *
 *   - 写出：UB + DataCopy → GM（X12）；禁 GlobalTensor::SetValue 写业务 GM。
 *   - 形态：KERNEL_TYPE_AIV_ONLY、blockDim=1；无 CrossCore / 无 flag 5/7。
 * 未抄 T25–T27 / alg15|21 / examples / frozen；原语：Sha3OneShot(mdlen=64)。
 */

#include "kernel_operator.h"
#include "fips203_device_sha3.hpp"

namespace {
/** m' / h / K' / r' 各 32B（FIPS 203 ML-KEM 消息与 G 半截） */
constexpr uint32_t kHashHalf = 32U;
/** G 输入 m'‖h = 64B；G 输出 K'‖r' = 64B */
constexpr uint32_t kGIoBytes = 64U;
/** SHA3-512 摘要长度（字节） */
constexpr int kSha3_512MdLen = 64;
}  // namespace

/**
 * AIV-only：G(m'‖h) → (K'‖r')，各 32B uint8。
 *
 * @param mPrimeGm 输入 m'[32]（Decrypt INTT/extract 终产物；本刀 H2D 直喂）
 * @param hGm      输入 h[32]；契约语义 = dk_kem[3104:3136) 切片（禁默认可重算 H(ek)）
 * @param kPrimeGm 输出 K'[32]；Host 不预喂；仅设备经 UB+DataCopy 写出
 * @param rPrimeGm 输出 r'/coins[32]；同上
 * 前置条件：blockDim=1；KERNEL_TYPE_AIV_ONLY；无 MIX / CrossCore。
 *
 * 数学：SHA3-512(m' ‖ h) → 64B；前 32B=K'，后 32B=r'（FIPS 203 G）。
 */
extern "C" __global__ __aicore__ void enc_g_custom(GM_ADDR mPrimeGm, GM_ADDR hGm,
                                                      GM_ADDR kPrimeGm, GM_ADDR rPrimeGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    AscendC::GlobalTensor<uint8_t> gmM;
    AscendC::GlobalTensor<uint8_t> gmH;
    AscendC::GlobalTensor<uint8_t> gmK;
    AscendC::GlobalTensor<uint8_t> gmR;
    gmM.SetGlobalBuffer((__gm__ uint8_t *)mPrimeGm, kHashHalf);
    gmH.SetGlobalBuffer((__gm__ uint8_t *)hGm, kHashHalf);
    gmK.SetGlobalBuffer((__gm__ uint8_t *)kPrimeGm, kHashHalf);
    gmR.SetGlobalBuffer((__gm__ uint8_t *)rPrimeGm, kHashHalf);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queInM;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queInH;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOutK;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOutR;
    // DataCopy 按字节：各缓冲 32B，满足 32B 对齐块。
    pipe.InitBuffer(queInM, 1, kHashHalf);
    pipe.InitBuffer(queInH, 1, kHashHalf);
    pipe.InitBuffer(queOutK, 1, kHashHalf);
    pipe.InitBuffer(queOutR, 1, kHashHalf);

    AscendC::LocalTensor<uint8_t> mLocal = queInM.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> hLocal = queInH.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> kLocal = queOutK.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> rLocal = queOutR.AllocTensor<uint8_t>();

    // ---- 1) GM → UB：m' 与 h（切片语义由 Host/golden 保证；本核不重算 H(ek)）----
    AscendC::DataCopy(mLocal, gmM, kHashHalf);
    AscendC::DataCopy(hLocal, gmH, kHashHalf);
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- 2) 拼 m'‖h 到栈缓冲；Sha3OneShot 不接 __gm__* ----
    // 背景：shared fips203_device_sha3 标量路径；输入/输出均为 UB/栈 uint8_t*。
    uint8_t mh[kGIoBytes];
    uint8_t gOut[kGIoBytes];
    for (uint32_t i = 0; i < kHashHalf; ++i) {
        mh[i] = mLocal.GetValue(i);
        mh[kHashHalf + i] = hLocal.GetValue(i);
    }

    // G = SHA3-512(m'‖h) → 64B
    F203SeDeviceKeccak::Sha3OneShot(gOut, kSha3_512MdLen, mh, kGIoBytes);

    // ---- 3) 拆 K'‖r' 写入 UB，再 DataCopy → GM（X12；禁 GM SetValue）----
    for (uint32_t i = 0; i < kHashHalf; ++i) {
        kLocal.SetValue(i, gOut[i]);
        rLocal.SetValue(i, gOut[kHashHalf + i]);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmK, kLocal, kHashHalf);
    AscendC::DataCopy(gmR, rLocal, kHashHalf);

    queInM.FreeTensor(mLocal);
    queInH.FreeTensor(hLocal);
    queOutK.FreeTensor(kLocal);
    queOutR.FreeTensor(rLocal);
}
