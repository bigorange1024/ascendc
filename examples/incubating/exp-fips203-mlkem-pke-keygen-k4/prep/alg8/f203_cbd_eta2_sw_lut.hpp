// @probe exp-fips203-mlkem-pke-keygen-k4
// @file prep/alg8/f203_cbd_eta2_sw_lut.hpp
// @layer prep
// @role prep/alg8：η=2 centered binomial（secret/noise）设备采样与 LUT；与 presample/alg7 链接成 prep 链。 / Alg.8 η=2 CBD prep helpers. 本文件 `f203_cbd_eta2_sw_lut.hpp` 为该子模块组件。 / Component: f203_cbd_eta2_sw_lut.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: cbd2_ab_lut.h, kernel_operator.h, cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 Alg.8 CBD_η=2：ŝ/ê 采样。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/alg8/f203_cbd_eta2_sw_lut.hpp
 */
/**
 * @file f203_cbd_eta2_sw_lut.hpp
 * @brief P1a：SWAR 比特展开 + 16 项 LUT（无分支、无运行时 mod Q）。
 *
 * 算法（与 mlkem-native / fips203_se_sample.c 同式）：
 *   t = load32_le(prf + 4*i)
 *   d = (t & 0x55555555) + ((t >> 1) & 0x55555555)   // 4×uint32 含 8 组 (a,b)
 *   coeff[j] = CBD2_AB_LUT[(a<<2)|b]
 *
 * 相对 P0：去掉内层 j 循环与 `% 3329`；实测总 tick 仅 −9.8%（GM scalar I/O 未动）。
 * 详见 docs/notes/F203-CBD-eta2-性能优化技术总结.md §3.1。
 */
#pragma once

#include "cbd2_ab_lut.h"

#include "kernel_operator.h"

#include <cstdint>

namespace F203CbdEta2 {

/** 从 SWAR 字 d 的第 j 组 4-bit 半系数提取 LUT 索引。 */
__aicore__ inline uint32_t Cbd2AbIndex(uint32_t d, uint32_t j)
{
    const uint32_t shift = 4U * j;
    const uint32_t a = (d >> shift) & 0x3U;
    const uint32_t b = (d >> (shift + 2U)) & 0x3U;
    return (a << 2U) | b;
}

/** 单 poly：栈缓冲区版（P1a scalar I/O 路径调用）。 */
__aicore__ inline void SamplePolyCbd2RowSwLut(int32_t *dst_row, const uint8_t *prf_row)
{
    for (uint32_t i = 0; i < N / 8U; ++i) {
        const uint32_t t = Load32Le(prf_row + 4U * i);
        const uint32_t d = (t & 0x55555555U) + ((t >> 1) & 0x55555555U);
        const uint32_t base = 8U * i;
        /* 8 系数展开：避免内层 j 循环与分支（编译器可进一步全展开 32×块）。 */
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

/** UB LocalTensor 读 4 字节（DataCopy 入 UB 后使用）。 */
__aicore__ inline uint32_t Load32LeUb(const AscendC::LocalTensor<uint8_t> &buf, uint32_t off)
{
    return static_cast<uint32_t>(buf.GetValue(off)) | (static_cast<uint32_t>(buf.GetValue(off + 1U)) << 8) |
           (static_cast<uint32_t>(buf.GetValue(off + 2U)) << 16) |
           (static_cast<uint32_t>(buf.GetValue(off + 3U)) << 24);
}

/** 单 poly：UB 版（P1b/P2 DataCopy 路径；结果写 rowQue 再 DataCopy 到 GM）。 */
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
