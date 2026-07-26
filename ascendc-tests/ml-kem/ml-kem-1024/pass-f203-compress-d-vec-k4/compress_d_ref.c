/**
 * @file compress_d_ref.c
 * @brief FIPS 203 Compress_d 的 host 侧标量参考实现（golden 计算内核）。
 *
 * 本文件在流水线中的位置：由 scripts/gen_data.py 编译为 libcompress_d_ref.so 并通过
 * ctypes 调用，产出 output/golden_comp.bin；不参与 AscendC 设备端编译，仅作 CPU/SIM 对拍
 * 的黑盒 oracle（按仓库规则，golden 只验 I/O，不是设备实现须复刻的规格）。
 * 规范对齐：FIPS 203 §4.2.1 Compress_d（Eq 4.7）等价整数形式，Barrett 乘数与移位量取自
 * mlkem-native 惯用常数（见 IMPLEMENTATION_PLAN.md §1 表格）。
 */
#include "compress_d_ref.h"

#include <stdint.h>

// d=4：Compress_4(u) = round(16u/q) mod 16。
// 1290160 = floor(2^4 * 2^32 / q) 的 Barrett 乘数（q=3329）；bias=2^27 对应右移 28 位的四舍五入。
uint32_t f203_scalar_compress_d4(uint32_t u)
{
    const uint32_t d0 = u * 1290160u;
    return (d0 + (1u << 27)) >> 28;
}

// d=5：Compress_5(u) = round(32u/q) mod 32。
// 1290176 = floor(2^5 * 2^32 / q) 的 Barrett 乘数；右移 27 位后先得到未截断商，再与 0x1f 做低 5 位取模。
uint32_t f203_scalar_compress_d5(uint32_t u)
{
    const uint32_t d0 = u * 1290176u;
    return ((d0 + (1u << 26)) >> 27) & 0x1fu;
}

// d=10：Compress_10(u) = round(1024u/q) mod 1024。乘积超出 u32 范围，须用 u64 承载。
// 2642263040 = floor(2^10 * 2^33 / q) 的 Barrett 乘数；右移 33 位后与 0x3ff 取低 10 位模。
uint32_t f203_scalar_compress_d10(uint32_t u)
{
    uint64_t d0 = (uint64_t)u * 2642263040ull;
    d0 = (d0 + ((uint64_t)1u << 32)) >> 33;
    return (uint32_t)(d0 & 0x3ffu);
}

// d=11：Compress_11(u) = round(2048u/q) mod 2048，同 d=10 需 u64 宽乘。
// 5284526080 = floor(2^11 * 2^33 / q) 的 Barrett 乘数；右移 33 位后与 0x7ff 取低 11 位模。
uint32_t f203_scalar_compress_d11(uint32_t u)
{
    uint64_t d0 = (uint64_t)u * 5284526080ull;
    d0 = (d0 + ((uint64_t)1u << 32)) >> 33;
    return (uint32_t)(d0 & 0x7ffu);
}

// 按 d 分派到对应 Barrett 标量实现；入参先夹到 [0, q-1] canonical 范围（防越界输入）。
static uint32_t scalar_compress(uint32_t u, int d)
{
    if (u >= (uint32_t)F203_MLKEM_Q) {
        u = (uint32_t)F203_MLKEM_Q - 1u;
    }
    switch (d) {
    case 4:
        return f203_scalar_compress_d4(u);
    case 5:
        return f203_scalar_compress_d5(u);
    case 10:
        return f203_scalar_compress_d10(u);
    case 11:
        return f203_scalar_compress_d11(u);
    default:
        return 0u;
    }
}

// 整 poly 循环：逐系数调用 scalar_compress；in/out 均为长度 n 的 host 内存数组（非 GM 指针）。
static void poly_compress_ref(int32_t *out, const int32_t *in, int32_t n, int d)
{
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t u = (uint32_t)in[i];
        out[i] = (int32_t)scalar_compress(u, d);
    }
}

void poly_compress_d4_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_compress_ref(out, in, n, 4);
}

void poly_compress_d5_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_compress_ref(out, in, n, 5);
}

void poly_compress_d10_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_compress_ref(out, in, n, 10);
}

void poly_compress_d11_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_compress_ref(out, in, n, 11);
}
