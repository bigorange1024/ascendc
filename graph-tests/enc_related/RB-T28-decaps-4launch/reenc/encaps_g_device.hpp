/**
 * @file encaps_g_device.hpp
 * @brief RB-T23：设备侧 Encaps 头 (K‖r)←G(m‖H(ek))（shared SHA3 积木）。
 *
 * 契约（FIPS 203 Alg.17 外形，与 gen_data / Host golden 一致）：
 *   h ← SHA3-256(ek)           // H，ek=1568B
 *   (K ‖ r) ← SHA3-512(m ‖ h)  // G，输入 64B，输出 64B
 *   coins := r                 // 喂后续 CBD（禁止 Host 预喂 coins/K）
 *
 * 实现：`library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp` 的
 * `F203SeDeviceKeccak::Sha3OneShot`（标量 Keccak-f[1600]）。
 * Sha3OneShot 消费 UB/栈上 uint8_t*，故先从 GM 拷入局部缓冲。
 *
 * 背景：相对 T21，把 Host 的 G/H 下沉到 Launch1 prep；未采用 Host 转发 K。
 */
#pragma once

#include "fips203_device_sha3.hpp"
#include "kernel_operator.h"
#include "reenc/tiling.h"

namespace EncapsGDevice {

/**
 * AIV：读 m[32]、ek[1568]，写 h/K/coins 到 workspace。
 * @param mGm     [in]  Host 预装消息 m ∈ B^32
 * @param ekGm    [in]  完整 ek_PKE（已镜像 OFF_EK 或与之同内容）
 * @param hOutGm  [out] H(ek)[32]
 * @param kOutGm  [out] K[32] = G(m‖h)[:32]
 * @param coinsGm [out] r/coins[32] = G(m‖h)[32:]
 * 前置：仅 AIV 调用；blockDim=1。
 */
__aicore__ inline void RunEncapsHeadG(const __gm__ uint8_t *mGm, const __gm__ uint8_t *ekGm,
                                      __gm__ uint8_t *hOutGm, __gm__ uint8_t *kOutGm,
                                      __gm__ uint8_t *coinsGm)
{
    using namespace enc_tiling;

    // ---- 1) ek → UB，H(ek)=SHA3-256 ----
    // 背景：Sha3OneShot 不接 __gm__*；1568B 与 KeyGen 尾 H(ek) 同式栈缓冲。
    uint8_t ekUb[kEkBytes];
    for (uint32_t i = 0; i < static_cast<uint32_t>(kEkBytes); ++i) {
        ekUb[i] = ekGm[i];
    }
    uint8_t h[kHashBytes];
    F203SeDeviceKeccak::Sha3OneShot(h, static_cast<int>(kHashBytes), ekUb,
                                    static_cast<uint32_t>(kEkBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- 2) 拼 m‖h（64B），G=SHA3-512 → K‖r ----
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

    // ---- 3) 写回 GM：h / K / coins(r) ----
    for (uint32_t i = 0; i < static_cast<uint32_t>(kHashBytes); ++i) {
        hOutGm[i] = h[i];
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(kSharedKeyBytes); ++i) {
        kOutGm[i] = gOut[i];
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(kCoinsBytes); ++i) {
        coinsGm[i] = gOut[static_cast<uint32_t>(kSharedKeyBytes) + i];
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * 从 workspace 偏移调用：读 OFF_M / OFF_EK，写 OFF_H / OFF_K / OFF_COINS。
 * @param ws 共享 workspace 基址
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
