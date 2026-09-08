/**
 * @file encaps_g_device.hpp
 * @brief RB-T27 reenc/Encaps：设备侧 (K‖r)←G(m‖H(ek))（shared SHA3）。
 *
 * 契约（FIPS 203 Alg.17 外形）：
 *   h ← SHA3-256(ek)； (K ‖ r) ← SHA3-512(m ‖ h)；coins := r
 *
 * X12：h/K/coins **写出**用 UB + DataCopy（禁依赖 GlobalTensor::SetValue / 标量 GM 写）。
 * 背景：相对 T23/T26 标量字节写回；本刀按反卡死检查单 §5 #8 改为 DataCopy。
 */
#pragma once

#include "fips203_device_sha3.hpp"
#include "kernel_operator.h"
#include "tiling.h"

namespace EncapsGDevice {

/**
 * 将栈上 uint8[len] 经 UB DataCopy 落到 GM（X12）。
 * @param dstGm  [out] GM 目标
 * @param src    [in]  栈/UB 源
 * @param len    字节数（须满足 DataCopy 对齐；本路径 32）
 */
__aicore__ inline void DataCopyU8ToGm(__gm__ uint8_t *dstGm, const uint8_t *src, uint32_t len)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que;
    pipe.InitBuffer(que, 1, len);
    AscendC::LocalTensor<uint8_t> loc = que.AllocTensor<uint8_t>();
    for (uint32_t i = 0; i < len; ++i) {
        loc.SetValue(i, src[i]);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::GlobalTensor<uint8_t> gm;
    gm.SetGlobalBuffer(dstGm, len);
    AscendC::DataCopy(gm, loc, len);
    AscendC::PipeBarrier<PIPE_ALL>();
    que.FreeTensor(loc);
}

/**
 * AIV：读 m[32]、ek[1568]，写 h/K/coins 到 workspace。
 * @param mGm     [in]  Host 预装消息 m ∈ B^32
 * @param ekGm    [in]  完整 ek_PKE
 * @param hOutGm  [out] H(ek)[32]
 * @param kOutGm  [out] K[32]
 * @param coinsGm [out] r/coins[32]
 * 前置：仅 AIV；blockDim=1。
 */
__aicore__ inline void RunEncapsHeadG(const __gm__ uint8_t *mGm, const __gm__ uint8_t *ekGm,
                                      __gm__ uint8_t *hOutGm, __gm__ uint8_t *kOutGm,
                                      __gm__ uint8_t *coinsGm)
{
    using namespace enc_tiling;

    // ---- 1) ek → 栈，H(ek)=SHA3-256 ----
    uint8_t ekUb[kEkBytes];
    for (uint32_t i = 0; i < static_cast<uint32_t>(kEkBytes); ++i) {
        ekUb[i] = ekGm[i];
    }
    uint8_t h[kHashBytes];
    F203SeDeviceKeccak::Sha3OneShot(h, static_cast<int>(kHashBytes), ekUb,
                                    static_cast<uint32_t>(kEkBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- 2) 拼 m‖h，G=SHA3-512 → K‖r ----
    uint8_t mh[kMsgBytes + kHashBytes];
    for (uint32_t i = 0; i < static_cast<uint32_t>(kMsgBytes); ++i) {
        mh[i] = mGm[i];
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(kHashBytes); ++i) {
        mh[static_cast<uint32_t>(kMsgBytes) + i] = h[i];
    }
    uint8_t gOut[kGOutBytes];
    F203SeDeviceKeccak::Sha3OneShot(gOut, static_cast<int>(kGOutBytes), mh,
                                    static_cast<uint32_t>(kMsgBytes + kHashBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- 3) X12：UB+DataCopy 写回 h / K / coins ----
    DataCopyU8ToGm(hOutGm, h, static_cast<uint32_t>(kHashBytes));
    DataCopyU8ToGm(kOutGm, gOut, static_cast<uint32_t>(kSharedKeyBytes));
    DataCopyU8ToGm(coinsGm, gOut + kSharedKeyBytes, static_cast<uint32_t>(kCoinsBytes));
}

/**
 * 从 workspace 偏移调用：读 OFF_M / OFF_EK，写 OFF_H / OFF_K / OFF_COINS。
 */
__aicore__ inline void RunEncapsHeadGFromWs(GM_ADDR ws)
{
    using namespace enc_tiling;
    const __gm__ uint8_t *mGm = reinterpret_cast<const __gm__ uint8_t *>(ws + OFF_M);
    const __gm__ uint8_t *ekGm = reinterpret_cast<const __gm__ uint8_t *>(ws + OFF_EK);
    __gm__ uint8_t *hGm = reinterpret_cast<__gm__ uint8_t *>(ws + OFF_H);
    __gm__ uint8_t *kGm = reinterpret_cast<__gm__ uint8_t *>(ws + OFF_K);
    __gm__ uint8_t *coinsGm = reinterpret_cast<__gm__ uint8_t *>(ws + OFF_COINS);
    RunEncapsHeadG(mGm, ekGm, hGm, kGm, coinsGm);
}

} // namespace EncapsGDevice
