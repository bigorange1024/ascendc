// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/byte_encode12_pair.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `byte_encode12_pair.hpp` 为该子模块组件。 / Component: byte_encode12_pair.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: byte_encode12_config.hpp, hat_line18_2s1e.hpp, kernel_operator.h, tiling.h, byte_encode12_vec.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

/**
 * @file byte_encode12_pair.hpp
 * @brief ByteEncode₁₂ 统一入口：标量 poly_byte_encode12_scalar 与向量 poly_byte_encode12_vec_local 分发。
 *
 * 用途：行 19–20 在 UB 上将 int32[256] 编码为 uint8[384]；poly_byte_encode12_local 按 BYTE_ENCODE12_VEC 选型。
 *
 * 调用方：`2s1e_post_ntt_ub.hpp`（mixPass=0/7、HAT_BYTE_ENCODE=1）。
 *
 * 不变量：coeffN=256；pairs=n/2；每对 (t0,t1) 各 12 bit 交织为 3 字节；含 hat_line18_2s1e 分片常量。
 *
 * Golden：byte_encode12_ref.c；verify_result.py 对 ek_out/sk_out。
 *
 * CMake：BYTE_ENCODE12_VEC（0=标量回退，1=向量默认）。
 */
#ifndef BYTE_ENCODE12_PAIR_HPP
#define BYTE_ENCODE12_PAIR_HPP

#include "byte_encode12_config.hpp"
#include "hat_line18_2s1e.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#if BYTE_ENCODE12_VEC >= 1
#include "byte_encode12_vec.hpp"
#endif

namespace byte_encode12 {

#if BYTE_ENCODE12_VEC < 1
constexpr uint32_t kPolyBytes = 384U;
constexpr uint32_t kPolysPerAiv = static_cast<uint32_t>(tiling::kEPerAiv);
constexpr uint32_t kAivShardBytes = kPolysPerAiv * kPolyBytes;
constexpr uint32_t kVecScratchBytes = 0U;
constexpr uint32_t kVecScratchInt32Slots = 0U;
#endif

/** 标量 ByteEncode₁₂：每对系数 12 bit → 3 字节（FIPS Alg.5） */
__aicore__ inline void poly_byte_encode12_scalar(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a, uint32_t coeffN)
{
    const uint32_t pairs = coeffN / 2U;
    for (uint32_t i = 0; i < pairs; ++i) {
        const uint16_t t0 = static_cast<uint16_t>(a.GetValue(2U * i) & 0xFFF);
        const uint16_t t1 = static_cast<uint16_t>(a.GetValue(2U * i + 1U) & 0xFFF);
        r.SetValue(3U * i + 0U, static_cast<uint8_t>(t0 & 0xFFU));
        r.SetValue(3U * i + 1U, static_cast<uint8_t>((t0 >> 8) | ((t1 << 4) & 0xF0U)));
        r.SetValue(3U * i + 2U, static_cast<uint8_t>(t1 >> 4));
    }
}

/** 按 BYTE_ENCODE12_VEC 分发标量/向量编码；encodeWs 仅向量路径使用 */
__aicore__ inline void poly_byte_encode12_local(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a, uint32_t coeffN,
                                                LocalTensor<int32_t> &encodeWs)
{
#if BYTE_ENCODE12_VEC >= 1 && BYTE_ENCODE12_PREFETCH >= 1
    poly_byte_encode12_prefetch_local(r, a, coeffN, encodeWs);
#elif BYTE_ENCODE12_VEC >= 1
    poly_byte_encode12_vec_local(r, a, coeffN, encodeWs);
#else
    (void)encodeWs;
    poly_byte_encode12_scalar(r, a, coeffN);
#endif
}

} // namespace byte_encode12

#endif
