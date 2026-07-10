/**
 * @file decompress_d_vec.hpp
 * @brief FIPS 203 Decompress_d 设备实现（d=4/5/10/11）。
 *
 * 本文件在流水线中的位置：被 decompress_d_custom.cpp 的 AIV-only kernel 直接 include，是
 * 探针唯一的设备端计算逻辑；decompress_d_custom.cpp 只负责 GM↔UB 搬运与调度，具体解压算法
 * 全部在本文件的 `decompress_d` 命名空间内实现。
 * 对齐规范：FIPS 203 §4.2.1 Decompress_d（Eq 4.8）；Decrypt 链用于 u'=Decompress_du(ByteDecode_du(c1))、
 * v=Decompress_dv(ByteDecode_dv(c2))（Alg.15 行 4）。
 * 与 golden 的关系：本文件标量分支与向量分支在数学上均等价于 decompress_d_ref.c 中同 d 的
 * 标量实现，设备输出须与 golden_poly.bin 逐系数一致。
 *
 * DECOMPRESS_D_VEC=1（默认）：Muls(q)+Adds(2^{d-1})+ShiftRight(d) 全 poly 向量；Decrypt 链推荐。
 * DECOMPRESS_D_VEC=0：标量 fallback。
 * 纯 per-lane，与 ByteDecode 标量 unpack 配对。见 docs/notes/F203-Compress-Decompress-向量实现指南.md、
 * docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md §4。
 */
#ifndef DECOMPRESS_D_VEC_HPP
#define DECOMPRESS_D_VEC_HPP

#include "decompress_d_config.hpp"
#include "f203_decompress_d_params.hpp"
#include "f203_mlkem_params.h"
#include "kernel_operator.h"

namespace decompress_d {

// 单个 ML-KEM 多项式的系数个数（=256，FIPS 203 §2.3），也是本探针一次 launch 处理的元素数。
constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);

// DECOMPRESS_D_VEC>=1：向量路径，全 poly 三条向量指令即可完成，无需 mask/宽乘（u*q 恒在 u32 安全范围内）。
#if DECOMPRESS_D_VEC >= 1

/**
 * 向量版 Decompress_d：out = (in * q + bias) >> d，对应 FIPS 203 Eq 4.8 的整数等价形式。
 * @param out 输出 int32 UB LocalTensor，长度 kPolyLen=256，写入解压后的 canonical mod q 系数 ∈ [0, q-1]。
 * @param in 输入 int32 UB LocalTensor，长度 kPolyLen，要求已是压缩域值 ∈ [0, 2^d-1]。
 * @param tmp int32 UB scratch LocalTensor，长度 kPolyLen，承载 (in*q+bias) 的中间乘加结果。
 * 前置条件：DECOMPRESS_D_VEC>=1（编译期选中向量路径）。bias/shift 由 f203_decompress_d_params.hpp
 * 按 F203_DECOMPRESS_D 派生（F203_DECOMPRESS_ROUND_BIAS / F203_DECOMPRESS_D_BITS）。
 * 数值安全性：u<2^11（d≤11）时 u*q < 2^11*3329 ≈ 6.8M，远小于 u32 上限，无需宽乘/mask。
 */
__aicore__ inline void poly_decompress_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                             AscendC::LocalTensor<int32_t> &tmp)
{
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t n = static_cast<int32_t>(kPolyLen);
    // tmp = in * q（放大到 q 倍，对应 Decompress 定义中的 q/2^d 缩放的分子部分）
    Muls(tmp, in, static_cast<int32_t>(F203_MLKEM_Q), n);
    // tmp += bias（四舍五入偏置 2^{d-1}）
    Adds(tmp, tmp, static_cast<int32_t>(F203_DECOMPRESS_ROUND_BIAS), n);
    // out = tmp >> d（右移得到四舍五入后的 canonical 系数，定义保证结果落在 [0,q-1]，无需再 mod）
    ShiftRight(out, tmp, F203_DECOMPRESS_D_BITS, n);
}

#else

/**
 * 标量 fallback（DECOMPRESS_D_VEC=0 时使用）：逐系数用 GetValue/SetValue 计算，
 * 与向量路径同一数学公式 (u*q+bias)>>d，仅用于对照验证，不是默认路径。
 * @param out/in 见向量版同名参数；第三个参数（tmp）在此路径下未被使用（保持函数签名一致）。
 */
__aicore__ inline void poly_decompress_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                             AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const uint32_t u = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(i),
                     static_cast<int32_t>((u * static_cast<uint32_t>(F203_MLKEM_Q) +
                                           static_cast<uint32_t>(F203_DECOMPRESS_ROUND_BIAS)) >>
                                          F203_DECOMPRESS_D_BITS));
    }
}

#endif

} // namespace decompress_d

#endif
