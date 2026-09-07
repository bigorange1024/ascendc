/**
 * @file compress_d_vec.hpp
 * @brief FIPS 203 Compress_d 设备实现（d=4/5/10/11）。
 *
 * 本文件在流水线中的位置：被 compress_d_custom.cpp 的 AIV-only kernel 直接 include，是
 * 探针唯一的设备端计算逻辑；compress_d_custom.cpp 只负责 GM↔UB 搬运与调度，具体压缩算法
 * 全部在本文件的 `compress_d` 命名空间内实现。
 * 对齐规范：FIPS 203 §4.2.1 Compress_d（Eq 4.7）；Encrypt 用于生成 c1=ByteEncode_du(Compress_du(u))、
 * c2=ByteEncode_dv(Compress_dv(v))（Alg.14 行 22–23）。
 * 与 golden 的关系：本文件的标量分支（scalar_compress_u32）与 compress_d_ref.c 逐 d 数学同构，
 * 向量分支（Barrett/cast_div）在数学上等价于标量分支，两者输出须与 golden_comp.bin 逐系数一致。
 *
 * COMPRESS_D_VEC=1（默认）：per-lane 向量（Barrett d=4/5 或 cast_div 商 d=10/11）；Encrypt tail 抄此路径。
 * COMPRESS_D_VEC=0：标量 fallback，仅对照。
 * 与 ByteEncode 不同：无 bit shuffle → 默认**激活**向量。见 docs/notes/F203-Compress-Decompress-向量实现指南.md。
 */
#ifndef COMPRESS_D_VEC_HPP
#define COMPRESS_D_VEC_HPP

#include "compress_d_config.hpp"
#include "f203_compress_d_params.hpp"
#include "f203_mlkem_params.h"
#include "kernel_operator.h"

namespace compress_d {

// 单个 ML-KEM 多项式的系数个数（=256，FIPS 203 §2.3），也是本探针一次 launch 处理的元素数。
constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);

/**
 * 单系数标量 Compress_d：仅在 COMPRESS_D_VEC=0（标量 fallback）路径下被 poly_compress_scalar 调用。
 * @param u 输入系数，要求已是 canonical mod q（[0, q-1]），函数内部不做越界钳位。
 * @return 压缩域值，范围 [0, 2^d-1]。
 * 四个分支按 F203_COMPRESS_D 编译期选择，与 compress_d_ref.c 中同名 f203_scalar_compress_d* 逐位一致
 * （常数来源同见该文件注释：Barrett 乘数 = floor(2^d * 2^N / q)，右移 N 位，d=4/5 需再 mask 低 d 位）。
 */
__aicore__ inline uint32_t scalar_compress_u32(uint32_t u)
{
#if F203_COMPRESS_D == 4
    // round(16u/q) = (u*1290160 + 2^27) >> 28，移位后天然落在 [0,15]，无需再 mask。
    const uint32_t d0 = u * 1290160u;
    return (d0 + (1u << 27)) >> 28;
#elif F203_COMPRESS_D == 5
    // round(32u/q) mod 32：右移 27 位得到未截断商后，与 0x1f 取低 5 位模。
    const uint32_t d0 = u * 1290176u;
    return ((d0 + (1u << 26)) >> 27) & 0x1fu;
#elif F203_COMPRESS_D == 10
    // round(1024u/q) mod 1024：乘积超出 u32，需用 u64 承载中间结果再右移 33 位、取低 10 位模。
    uint64_t d0 = static_cast<uint64_t>(u) * 2642263040ull;
    d0 = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<uint32_t>(d0 & 0x3ffu);
#elif F203_COMPRESS_D == 11
    // round(2048u/q) mod 2048：同 d=10，u64 宽乘 + 右移 33 位 + 取低 11 位模。
    uint64_t d0 = static_cast<uint64_t>(u) * 5284526080ull;
    d0 = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<uint32_t>(d0 & 0x7ffu);
#endif
}

/**
 * 向量版「取低 bits 位模」：v mod 2^bits，原地更新 v；tmp 为同长度 scratch 缓冲，运算后不含有效数据。
 * @param v 待取模的 int32 UB LocalTensor，长度 = count，原地写回结果。
 * @param tmp int32 UB scratch LocalTensor，长度 = count，仅供本函数内部中间值使用。
 * @param bits 取模位宽 d（即保留低 bits 位）。
 * @param count 参与运算的元素个数（本探针恒为 kPolyLen=256）。
 * 等价于 v - floor(v/2^bits)*2^bits：先算出高位商（右移 bits），再乘回 2^bits 并从原值中减去，
 * 从而只保留低 bits 位——AscendC 向量 ISA 无原生「按位与常量」指令，故用「移位+乘+减」模拟 mask。
 */
__aicore__ inline void mask_low_bits_i32(AscendC::LocalTensor<int32_t> &v, AscendC::LocalTensor<int32_t> &tmp,
                                         int32_t bits, uint32_t count)
{
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;
    const int32_t n = static_cast<int32_t>(count);
    const int32_t scale = static_cast<int32_t>(1) << bits;
    ShiftRight(tmp, v, bits, n);   // tmp = v >> bits（高位商，即需要被清掉的部分）
    Muls(tmp, tmp, scale, n);      // tmp = (v >> bits) << bits（对齐回原量级的高位部分）
    Sub(v, v, tmp, n);             // v = v - tmp，等价于只保留原 v 的低 bits 位
}

#if !F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1

/**
 * int32 Barrett 向量：d=4/5（magic 乘积可放进 u32 lane，无需 64-bit 宽乘）。
 * @param out 输出 int32 UB LocalTensor，长度 kPolyLen=256，写入压缩域结果 ∈ [0, 2^d-1]。
 * @param in 输入 int32 UB LocalTensor，长度 kPolyLen，要求 canonical mod q（[0, q-1]）系数。
 * @param tmp int32 UB scratch LocalTensor，长度 kPolyLen，供 mask_low_bits_i32 内部使用。
 * 前置条件：F203_COMPRESS_USE_CAST_DIV=0（即 d=4 或 5），否则本函数不会被编译进本 TU。
 * 三条向量指令对应标量 Barrett 公式 (u*MUL + BIAS) >> SHIFT，其中 MUL/BIAS/SHIFT 由
 * f203_compress_d_params.hpp 按 d 展开（与 compress_d_ref.c 标量实现同一组常数）。
 */
__aicore__ inline void poly_compress_barrett_vec(AscendC::LocalTensor<int32_t> &out,
                                                 AscendC::LocalTensor<int32_t> &in,
                                                 AscendC::LocalTensor<int32_t> &tmp)
{
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t n = static_cast<int32_t>(kPolyLen);
    // out = in * BARRETT_MUL（u32 环境下允许溢出 wrap，这是 Barrett 近似乘法的设计前提，不可改为饱和/宽类型）
    Muls(out, in, static_cast<int32_t>(F203_COMPRESS_BARRETT_MUL), n);
    // out += BARRETT_BIAS（四舍五入所需的舍入偏置，d=4 为 2^27，d=5 为 2^26）
    Adds(out, out, static_cast<int32_t>(F203_COMPRESS_BARRETT_BIAS), n);
    // out >>= BARRETT_SHIFT（右移得到未截断的压缩域商）
    ShiftRight(out, out, F203_COMPRESS_BARRETT_SHIFT, n);
    // d=5 时移位商未必落在 [0,31]，需再取低 D_BITS 位模；d=4 该调用是幂等的（已在范围内）。
    mask_low_bits_i32(out, tmp, F203_COMPRESS_D_BITS, kPolyLen);
}

#endif

#if F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1

/**
 * cast_div 商向量：d=10/11。
 * round(u·2^d/q) = floor((u·2^d + q/2)/q)；与 liboqs Barrett 标量全 u∈[0,q) 0 差异。
 * @param out 输出 int32 UB LocalTensor，长度 kPolyLen=256，写入压缩域结果 ∈ [0, 2^d-1]。
 * @param in 输入 int32 UB LocalTensor，长度 kPolyLen，canonical mod q 系数。
 * @param tmp_i int32 UB scratch LocalTensor，长度 kPolyLen；先承载整数中间值，后被
 *              mask_low_bits_i32 复用为其内部 scratch。
 * @param fRaw/fTmp/fQuot 三段 float UB scratch LocalTensor，长度均为 kPolyLen，互不重叠
 *        （由 compress_d_custom.cpp 在同一块 f_buf 上按偏移切出）。
 * 前置条件：F203_COMPRESS_USE_CAST_DIV=1（即 d=10 或 11），d=10/11 的 Barrest 乘数超出 u32
 * 安全范围，故改用「整数扩大 2^d 倍 + 转 float 除法求商 + 截断转回整数」的等价实现，
 * 避免在向量 ISA 上手写 64-bit 宽乘。q=3329、2^d（d≤11）均在 float32 精确可表范围内，
 * 故此路径不会引入额外舍入误差。
 */
__aicore__ inline void poly_compress_cast_div_vec(AscendC::LocalTensor<int32_t> &out,
                                                  AscendC::LocalTensor<int32_t> &in,
                                                  AscendC::LocalTensor<int32_t> &tmp_i,
                                                  AscendC::LocalTensor<float> &fRaw,
                                                  AscendC::LocalTensor<float> &fTmp,
                                                  AscendC::LocalTensor<float> &fQuot)
{
    using AscendC::Adds;
    using AscendC::Cast;
    using AscendC::Div;
    using AscendC::Duplicate;
    using AscendC::Muls;
    const int32_t n = static_cast<int32_t>(kPolyLen);
    const int32_t kScale = static_cast<int32_t>(1) << F203_COMPRESS_D_BITS;  // 2^d
    const int32_t kRoundBias = static_cast<int32_t>(F203_MLKEM_Q / 2);       // q/2，四舍五入偏置

    // tmp_i = u * 2^d + q/2（分子，对应 round(u*2^d/q) = floor((u*2^d + q/2)/q) 的分子部分）
    Muls(tmp_i, in, kScale, n);
    Adds(tmp_i, tmp_i, kRoundBias, n);
    // fRaw = float(tmp_i)：把分子转成浮点，为后续除法做准备
    Cast(fRaw, tmp_i, AscendC::RoundMode::CAST_NONE, static_cast<uint32_t>(n));
    // 用 Duplicate 把常量 q 广播进整型 tmp_i，再转 float 得到分母 fTmp（全 lane 相同值 q）
    Duplicate(tmp_i, static_cast<int32_t>(F203_MLKEM_Q), n);
    Cast(fTmp, tmp_i, AscendC::RoundMode::CAST_NONE, static_cast<uint32_t>(n));
    // fQuot = fRaw / fTmp：浮点除法得到未截断商
    Div(fQuot, fRaw, fTmp, n);
    // out = trunc(fQuot)：截断取整（对应整数除法的 floor，因分子分母皆非负）
    Cast(out, fQuot, AscendC::RoundMode::CAST_TRUNC, static_cast<uint32_t>(n));
    // 商可能超出 [0, 2^d-1]（如 u=q-1 时理论上 round 后等于 2^d），再 mask 低 D_BITS 位模，
    // 与 Compress_d 定义中的 "mod 2^d" 对齐。
    mask_low_bits_i32(out, tmp_i, F203_COMPRESS_D_BITS, kPolyLen);
}

#endif

/**
 * 逐系数标量 fallback（COMPRESS_D_VEC=0 时使用），也是 cast_div 分支未走向量时的兜底路径。
 * @param out/in 均为长度 kPolyLen 的 int32 UB LocalTensor；out 写入压缩域结果。
 * 用 GetValue/SetValue 逐 lane 读写（无向量指令），逻辑与标量参考 scalar_compress_u32 完全一致。
 */
__aicore__ inline void poly_compress_scalar(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in)
{
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const uint32_t u = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(scalar_compress_u32(u)));
    }
}

/**
 * d=4/5 场景下 compress_d_custom.cpp 调用的统一入口：按 COMPRESS_D_VEC 在向量 Barrett
 * 与标量 fallback 之间切换。d=10/11（USE_CAST_DIV=1）时退化为标量路径，实际向量计算改走
 * 下方 poly_compress_cast_div_dispatch（因为需要额外的 float scratch，参数列表不同）。
 * @param out/in/tmp 见 poly_compress_barrett_vec / poly_compress_scalar 对应参数说明。
 */
__aicore__ inline void poly_compress_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                           AscendC::LocalTensor<int32_t> &tmp)
{
#if !F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1
    poly_compress_barrett_vec(out, in, tmp);
#else
    (void)tmp;
    poly_compress_scalar(out, in);
#endif
}

// 仅当 d=10/11 且向量路径开启时才编译：为 poly_compress_cast_div_vec 提供与
// compress_d_custom.cpp 调用点参数个数匹配的薄转发层（其余分支下无需此函数，也不消耗 float buffer）。
#if F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1
__aicore__ inline void poly_compress_cast_div_dispatch(AscendC::LocalTensor<int32_t> &out,
                                                       AscendC::LocalTensor<int32_t> &in,
                                                       AscendC::LocalTensor<int32_t> &tmp_i,
                                                       AscendC::LocalTensor<float> &fRaw,
                                                       AscendC::LocalTensor<float> &fTmp,
                                                       AscendC::LocalTensor<float> &fQuot)
{
    poly_compress_cast_div_vec(out, in, tmp_i, fRaw, fTmp, fQuot);
}
#endif

} // namespace compress_d

#endif
