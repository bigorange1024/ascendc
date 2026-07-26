#ifndef BYTE_ENCODE12_PAIR_HPP
#define BYTE_ENCODE12_PAIR_HPP

/**
 * @file byte_encode12_pair.hpp
 * @brief 单 poly ByteEncode₁₂ 设备入口：标量实现 + 按配置分派到向量/prefetch。
 *
 * 流水线位置：AivByteEncode12Only::stageEncodeOut 对每个 ek/sk poly 调用本文件入口。
 * 与 golden 关系：标量路径与 byte_encode12_ref.c 同公式；向量路径 I/O 等价。
 * 作用：提供 poly_byte_encode12_scalar / poly_byte_encode12_local；VEC=0 时定义 scratch 尺寸常量。
 */

#include "byte_encode12_config.hpp"
#include "hat_line18_2s1e.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#if BYTE_ENCODE12_VEC >= 1
#include "byte_encode12_vec.hpp"
#endif

namespace byte_encode12 {

#if BYTE_ENCODE12_VEC < 1
/** 标量路径：无向量 scratch；尺寸常量仍供 only.hpp 分配 ek/sk 本地缓冲 */
constexpr uint32_t kPolyBytes = 384U;
constexpr uint32_t kPolysPerAiv = static_cast<uint32_t>(tiling::kEPerAiv);
constexpr uint32_t kAivShardBytes = kPolysPerAiv * kPolyBytes;
constexpr uint32_t kVecScratchBytes = 0U;
constexpr uint32_t kVecScratchInt32Slots = 0U;
#endif

/**
 * 标量 ByteEncode₁₂：UB 内 GetValue/SetValue，与 FIPS Alg.5 / ref.c 一致。
 * @param r      输出 LocalTensor<uint8>，长度 ≥ pairs*3（通常 384）
 * @param a      输入 LocalTensor<int32>，形状 [coeffN]
 * @param coeffN 系数个数（偶数，通常 256）
 * 前置条件：a/r 已在 UB；无向量 API。
 */
__aicore__ inline void poly_byte_encode12_scalar(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a, uint32_t coeffN)
{
    const uint32_t pairs = coeffN / 2U;
    // i：系数对下标；t0/t1 取低 12 bit 后打成 3 字节
    for (uint32_t i = 0; i < pairs; ++i) {
        const uint16_t t0 = static_cast<uint16_t>(a.GetValue(2U * i) & 0xFFF);
        const uint16_t t1 = static_cast<uint16_t>(a.GetValue(2U * i + 1U) & 0xFFF);
        r.SetValue(3U * i + 0U, static_cast<uint8_t>(t0 & 0xFFU));
        r.SetValue(3U * i + 1U, static_cast<uint8_t>((t0 >> 8) | ((t1 << 4) & 0xF0U)));
        r.SetValue(3U * i + 2U, static_cast<uint8_t>(t1 >> 4));
    }
}

/**
 * 单 poly 编码统一入口：按 BYTE_ENCODE12_VEC / PREFETCH 分派。
 * @param r        输出字节 LocalTensor
 * @param a        输入系数 LocalTensor[int32]
 * @param coeffN   系数数
 * @param encodeWs 向量/prefetch 工作区（标量路径忽略）
 * 前置条件：VEC≥1 时 encodeWs 已按 kVecScratch / kPrefetchScratch 分配。
 */
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
