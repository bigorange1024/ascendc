/**
 * @file f203_cbd_eta2_sw_lut.hpp
 * @brief SWAR 比特展开 + 16 项 LUT（η=2，无分支、无运行时 mod Q）。
 *
 * 算法（与 `golden_se_sampling.sample_poly_cbd2` 同语义）：
 *   t = load32_le(prf + 4*i)
 *   d = (t & 0x55555555) + ((t >> 1) & 0x55555555)
 *   coeff[j] = CBD2_AB_LUT[(a<<2)|b]
 *
 * 本文件在流水线中的位置：被 P1a 标量 I/O 路径与默认 DataCopy+UB 路径共同调用。
 */
#pragma once

#include "cbd2_ab_lut.h"

#include "kernel_operator.h"

#include <cstdint>

namespace F203CbdEta2 {

/**
 * 从 SWAR 字 d 的第 j 组 4-bit 半系数提取 LUT 索引。
 * @param d [in] SWAR 聚合字：每 4-bit 一组 (a,b)，a 占低 2 位，b 占高 2 位
 * @param j [in] 组号，范围 [0,8)（一个 32-bit 字含 8 组 (a,b)）
 * @return LUT 下标 `(a<<2)|b`，范围 [0,16)，用于索引 CBD2_AB_LUT
 */
__aicore__ inline uint32_t Cbd2AbIndex(uint32_t d, uint32_t j)
{
    const uint32_t shift = 4U * j;
    const uint32_t a = (d >> shift) & 0x3U;
    const uint32_t b = (d >> (shift + 2U)) & 0x3U;
    return (a << 2U) | b;
}

/**
 * 单 poly：栈缓冲区版（P1a scalar I/O 路径调用）。
 * @param dst_row [out] 单行输出，长度 N=256 的 int32 栈数组
 * @param prf_row [in]  单行 PRF 输出，长度 PRF_BYTES=128 的 uint8 栈数组
 */
__aicore__ inline void SamplePolyCbd2RowSwLut(int32_t *dst_row, const uint8_t *prf_row)
{
    for (uint32_t i = 0; i < N / 8U; ++i) {
        const uint32_t t = Load32Le(prf_row + 4U * i);
        /* SWAR：奇偶位分别求和，把交错排列的 2-bit 计数聚合为每 4-bit 一组的 (a,b) 对。 */
        const uint32_t d = (t & 0x55555555U) + ((t >> 1) & 0x55555555U);
        const uint32_t base = 8U * i;
        /* 8 系数展开：避免内层 j 循环与分支，减少设备 hot path 指令数。 */
        dst_row[base + 0U] = CBD2_AB_LUT[Cbd2AbIndex(d, 0U)];
        dst_row[base + 1U] = CBD2_AB_LUT[Cbd2AbIndex(d, 1U)];
        dst_row[base + 2U] = CBD2_AB_LUT[Cbd2AbIndex(d, 2U)];
        dst_row[base + 3U] = CBD2_AB_LUT[Cbd2AbIndex(d, 3U)];
        dst_row[base + 4U] = CBD2_AB_LUT[Cbd2AbIndex(d, 4U)];
        dst_row[base + 5U] = CBD2_AB_LUT[Cbd2AbIndex(d, 5U)];
        dst_row[base + 6U] = CBD2_AB_LUT[Cbd2AbIndex(d, 6U)];
        dst_row[base + 7U] = CBD2_AB_LUT[Cbd2AbIndex(d, 7U)];
    }
}

/**
 * UB LocalTensor 读 4 字节（DataCopy 入 UB 后使用），小端序拼装为 uint32_t。
 * @param buf [in] UB 上的 uint8 LocalTensor，须已通过 DataCopy 从 GM 填入有效数据
 * @param off [in] 起始偏移（字节），要求 off+3 未越界
 * @return 小端序拼装的 32-bit 值
 */
__aicore__ inline uint32_t Load32LeUb(const AscendC::LocalTensor<uint8_t> &buf, uint32_t off)
{
    return static_cast<uint32_t>(buf.GetValue(off)) | (static_cast<uint32_t>(buf.GetValue(off + 1U)) << 8) |
           (static_cast<uint32_t>(buf.GetValue(off + 2U)) << 16) |
           (static_cast<uint32_t>(buf.GetValue(off + 3U)) << 24);
}

/**
 * 单 poly：UB 版（默认 DataCopy 路径；结果写 rowQue 再 DataCopy 到 GM）。
 * @param dst_row [out] UB 上的 int32 LocalTensor，长度 N=256，写入该行 CBD 采样结果
 * @param prf_row [in]  UB 上的 uint8 LocalTensor，长度 PRF_BYTES=128，该行 PRF 输出
 */
__aicore__ inline void SamplePolyCbd2RowSwLutUb(AscendC::LocalTensor<int32_t> &dst_row,
                                                const AscendC::LocalTensor<uint8_t> &prf_row)
{
    for (uint32_t i = 0; i < N / 8U; ++i) {
        const uint32_t t = Load32LeUb(prf_row, 4U * i);
        const uint32_t d = (t & 0x55555555U) + ((t >> 1) & 0x55555555U);
        const uint32_t base = 8U * i;
        dst_row.SetValue(base + 0U, CBD2_AB_LUT[Cbd2AbIndex(d, 0U)]);
        dst_row.SetValue(base + 1U, CBD2_AB_LUT[Cbd2AbIndex(d, 1U)]);
        dst_row.SetValue(base + 2U, CBD2_AB_LUT[Cbd2AbIndex(d, 2U)]);
        dst_row.SetValue(base + 3U, CBD2_AB_LUT[Cbd2AbIndex(d, 3U)]);
        dst_row.SetValue(base + 4U, CBD2_AB_LUT[Cbd2AbIndex(d, 4U)]);
        dst_row.SetValue(base + 5U, CBD2_AB_LUT[Cbd2AbIndex(d, 5U)]);
        dst_row.SetValue(base + 6U, CBD2_AB_LUT[Cbd2AbIndex(d, 6U)]);
        dst_row.SetValue(base + 7U, CBD2_AB_LUT[Cbd2AbIndex(d, 7U)]);
    }
}

}  // namespace F203CbdEta2
