/**
 * @file f203_cbd_eta3_sw_lut.hpp
 * @brief CBD η=3 的 SWAR 位抽取 + 64 项 LUT（ML-KEM-512）。
 *
 * Alg.8 CBD_3 每 3 字节产生 4 个系数：
 *   t = load24_le(prf + 3*i)
 *   d = (t & 0x00249249) + ((t >> 1) & 0x00249249) + ((t >> 2) & 0x00249249)
 *   coeff[j] = CBD3_AB_LUT[(a<<3)|b]
 * 其中 a = d[6j+0 : 6j+3)，b = d[6j+3 : 6j+6)。这与 liboqs cbd3
 * 的位抽取对齐，输出落到 [0,q) 以匹配仓内 cbd2/cbd3 golden。
 */
#pragma once

#include "cbd3_ab_lut.h"

#include "kernel_operator.h"

#include <cstdint>

namespace F203CbdEta3 {

/**
 * 从 CBD_3 SWAR 聚合字 d 中提取第 j 个系数的 LUT 下标。
 * @param d SWAR 聚合字，每 6 bit 一组 (a,b)，a/b 均为 3-bit
 * @param j 组号，范围 [0,4)
 * @return `(a<<3)|b`，范围 [0,64)
 */
__aicore__ inline uint32_t Cbd3AbIndex(uint32_t d, uint32_t j)
{
    const uint32_t shift = 6U * j;
    const uint32_t a = (d >> shift) & 0x7U;
    const uint32_t b = (d >> (shift + 3U)) & 0x7U;
    return (a << 3U) | b;
}

/**
 * 单 poly 栈缓冲版：P1a scalar I/O 路径调用。
 * @param dst_row [out] 长度 N=256 的 int32 输出数组
 * @param prf_row [in]  长度 PRF_BYTES=192 的 uint8 PRF 行
 */
__aicore__ inline void SamplePolyCbd3RowSwLut(int32_t *dst_row, const uint8_t *prf_row)
{
    for (uint32_t i = 0; i < N / 4U; ++i) {
        const uint32_t t = Load24Le(prf_row + 3U * i);
        const uint32_t d = (t & 0x00249249U) + ((t >> 1) & 0x00249249U) + ((t >> 2) & 0x00249249U);
        const uint32_t base = 4U * i;
        dst_row[base + 0U] = CBD3_AB_LUT[Cbd3AbIndex(d, 0U)];
        dst_row[base + 1U] = CBD3_AB_LUT[Cbd3AbIndex(d, 1U)];
        dst_row[base + 2U] = CBD3_AB_LUT[Cbd3AbIndex(d, 2U)];
        dst_row[base + 3U] = CBD3_AB_LUT[Cbd3AbIndex(d, 3U)];
    }
}

/**
 * UB LocalTensor 小端读取 3 字节；默认 DataCopy 路径在 UB 中处理 PRF 行。
 * @param buf UB 上的 uint8 LocalTensor
 * @param off 起始字节偏移，要求 off+2 未越过当前 PRF 行
 */
__aicore__ inline uint32_t Load24LeUb(const AscendC::LocalTensor<uint8_t> &buf, uint32_t off)
{
    return static_cast<uint32_t>(buf.GetValue(off)) | (static_cast<uint32_t>(buf.GetValue(off + 1U)) << 8) |
           (static_cast<uint32_t>(buf.GetValue(off + 2U)) << 16);
}

/**
 * 单 poly UB 版：从 prf_row 读 192B，向 dst_row 写 256 个 int32 系数。
 * @param dst_row [out] UB 上的 int32 LocalTensor，长度 N=256
 * @param prf_row [in]  UB 上的 uint8 LocalTensor，长度 PRF_BYTES=192
 */
__aicore__ inline void SamplePolyCbd3RowSwLutUb(AscendC::LocalTensor<int32_t> &dst_row,
                                                const AscendC::LocalTensor<uint8_t> &prf_row)
{
    for (uint32_t i = 0; i < N / 4U; ++i) {
        const uint32_t t = Load24LeUb(prf_row, 3U * i);
        const uint32_t d = (t & 0x00249249U) + ((t >> 1) & 0x00249249U) + ((t >> 2) & 0x00249249U);
        const uint32_t base = 4U * i;
        dst_row.SetValue(base + 0U, CBD3_AB_LUT[Cbd3AbIndex(d, 0U)]);
        dst_row.SetValue(base + 1U, CBD3_AB_LUT[Cbd3AbIndex(d, 1U)]);
        dst_row.SetValue(base + 2U, CBD3_AB_LUT[Cbd3AbIndex(d, 2U)]);
        dst_row.SetValue(base + 3U, CBD3_AB_LUT[Cbd3AbIndex(d, 3U)]);
    }
}

}  // namespace F203CbdEta3
