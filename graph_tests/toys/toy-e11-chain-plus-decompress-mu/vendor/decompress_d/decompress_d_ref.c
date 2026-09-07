/**
 * @file decompress_d_ref.c
 * @brief FIPS 203 Decompress_d 的 host 侧标量参考实现（golden 计算内核）。
 *
 * 本文件在流水线中的位置：由 scripts/gen_data.py 编译为 libdecompress_d_ref.so 并通过
 * ctypes 调用，产出 output/golden_poly.bin；不参与 AscendC 设备端编译，仅作 CPU/SIM 对拍
 * 的黑盒 oracle（golden 只验 I/O，不是设备实现须复刻的规格）。
 * 规范对齐：FIPS 203 §4.2.1 Decompress_d（Eq 4.8）：Decompress_d(u) = round((q/2^d)·u)，
 * 整数形式 = ((u*q) + bias) >> d，bias=2^{d-1} 对应四舍五入偏置（见 IMPLEMENTATION_PLAN.md §1 表格）。
 */
#include "decompress_d_ref.h"

#include "f203_mlkem_params.h"

// 按 d 选取四舍五入偏置 bias=2^{d-1}（d=4→8, d=5→16, d=10→512, d=11→1024），
// 再对整 poly 做 (u*q + bias) >> d：先放大到 q 倍再右移 d 位，等价于四舍五入除以 2^d/q 的逆运算。
static void poly_decompress_ref(int32_t *out, const int32_t *in, int32_t n, int d)
{
    uint32_t bias = 0U;
    if (d == 4) {
        bias = 8U;
    } else if (d == 5) {
        bias = 16U;
    } else if (d == 10) {
        bias = 512U;
    } else if (d == 11) {
        bias = 1024U;
    }
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t u = (uint32_t)in[i];
        out[i] = (int32_t)((((uint32_t)u * (uint32_t)F203_MLKEM_Q) + bias) >> d);
    }
}

void poly_decompress_d4_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_decompress_ref(out, in, n, 4);
}

void poly_decompress_d5_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_decompress_ref(out, in, n, 5);
}

void poly_decompress_d10_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_decompress_ref(out, in, n, 10);
}

void poly_decompress_d11_ref(int32_t *out, const int32_t *in, int32_t n)
{
    poly_decompress_ref(out, in, n, 11);
}
