/**
 * @file f203_cbd_eta2.hpp
 * @brief FIPS 203 Alg.8 SamplePolyCBD_η=2 — batch 8 poly 入口与编译期契约。
 *
 * 数据流（P1b/P2 默认路径）：
 *   prf_out[8,128] GM ──DataCopy──► UB ──SWAR+LUT──► src[8,256] GM
 *
 * 编译开关（CMake）：
 *   -Dcbd_p0_scalar=ON          → P0 标量 CBD + scalar GM（对照）
 *   -Dcbd_p1a_scalar_io=ON      → P1a SWAR+LUT，仍 scalar GM
 *   -Dcbd_block_dim=1|2         → P1b-single / P2 双 AIV（探针 run.sh 默认 2）
 *
 * 布局与 vec-k4-v2 `src.bin` 一致；双 AIV 分片见 RowForBlock / LAYOUT.md。
 */
#pragma once

#include "kernel_operator.h"

#include <cstdint>

namespace F203CbdEta2 {

constexpr uint32_t K = 4U;         // ML-KEM-1024 模块秩 k
constexpr uint32_t N = 256U;       // 每个多项式系数个数
constexpr uint32_t Q = 3329U;      // FIPS 203 模数 q
constexpr uint32_t ETA = 2U;       // CBD 噪声参数 η（本探针固定 η=2）
/** PRF 输出字节 / poly：η·N/4 = 128。 */
constexpr uint32_t PRF_BYTES = (ETA * N) / 4U;
/** 8 行：前 K 行 s，后 K 行 e（语义标注；CBD 各行独立）。 */
constexpr uint32_t ROWS = 2U * K;
constexpr uint32_t PRF_TOTAL_BYTES = ROWS * PRF_BYTES;  // 8 行 PRF 总字节数：8*128=1024
constexpr uint32_t SRC_COEFFS = ROWS * N;               // 输出 src 系数总数：8*256=2048

/** P2：每 AIV 处理行数（8 行 / 2 个 AIV = 4 行）。 */
constexpr uint32_t ROWS_PER_AIV = ROWS / 2U;
constexpr uint32_t CBD_BLOCK_DIM = 2U;

/**
 * P2 行表 — 与 vec-k4-v2 Stage1（Aiv2s1eSplit）一致：
 *   AIV0 → ŝ 两行 + ê 前半；AIV1 → ŝ 两行 + ê 后半。
 * 数值来源：行号 0-3 对应 ŝ 的 4 个分量，4-7 对应 ê 的 4 个分量；本表把它们
 * 交叉分配到两个 AIV，使每个 AIV 同时持有 ŝ 与 ê 的部分行，与 vec-k4-v2 下游
 * 消费方（KeyGen prep）对分片方式的假设保持一致，便于将本探针结果直接迁移。
 */
constexpr uint32_t kAiv0Rows[ROWS_PER_AIV] = {0U, 1U, 4U, 5U};
constexpr uint32_t kAiv1Rows[ROWS_PER_AIV] = {2U, 3U, 6U, 7U};

/**
 * 由 blockIdx（0 或 1）与 AIV 内局部行号，查表得到全局 src/prf 行号（0..7）。
 * @param blockIdx  [in] 当前 AIV 的 block 编号（P2 下取值 0 或 1）
 * @param localRow  [in] 该 AIV 内的局部行索引，范围 [0, ROWS_PER_AIV)
 * @return 全局行号，范围 [0, ROWS)，用于定位 prf_gm/src_gm 中该行的偏移
 */
__aicore__ inline uint32_t RowForBlock(uint32_t blockIdx, uint32_t localRow)
{
    return (blockIdx == 0U) ? kAiv0Rows[localRow] : kAiv1Rows[localRow];
}

/**
 * 从裸指针按小端序读取 4 字节组成 uint32_t（栈/标量路径，P0/P1a 使用）。
 * @param buf [in] 指向至少 4 字节的缓冲区
 * @return 小端序拼装的 32-bit 值
 */
__aicore__ inline uint32_t Load32Le(const uint8_t *buf)
{
    return static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
}

}  // namespace F203CbdEta2

#include "f203_cbd_eta2_sw_lut.hpp"
#if !defined(F203_CBD_ETA2_P0_SCALAR) && !defined(F203_CBD_ETA2_P1A_SCALAR_IO)
#include "f203_cbd_eta2_ub_io.hpp"
#endif

namespace F203CbdEta2 {

#if defined(F203_CBD_ETA2_P0_SCALAR)
/**
 * P0 对照：逐系数分支 + `% Q`。
 * 保留供 `cbd_p0_scalar` 基准与 SIM 门控对照，非性能路径。
 * @param dst_row [out] 单行输出，长度 N=256 的 int32 数组（栈缓冲区，标量路径）
 * @param prf_row [in]  单行 PRF 输出，长度 PRF_BYTES=128 的 uint8 数组（η=2 CBD 输入）
 * 算法：每 4 字节（32-bit）打包 8 组 (a,b) 2-bit 半系数：
 *   t = load32_le(prf_row + 4*i)
 *   d = (t & 0x55555555) + ((t>>1) & 0x55555555)  —— SWAR 技巧：按位对提取偏移求和，
 *       把奇偶交错的 2-bit 计数聚合为每 4-bit 一组的汉明重量对 (a,b)
 *   对第 j 组：a = d 的第 [4j, 4j+2) 位，b = d 的第 [4j+2, 4j+4) 位
 *   c = (a - b) mod Q（此处用条件加 Q 后再 `%Q` 兜底，即本对照组的运行时分支+取模开销来源）
 */
__aicore__ inline void SamplePolyCbd2Row(int32_t *dst_row, const uint8_t *prf_row)
{
    for (uint32_t i = 0; i < N / 8U; ++i) {
        const uint32_t t = Load32Le(prf_row + 4U * i);
        const uint32_t d = (t & 0x55555555U) + ((t >> 1) & 0x55555555U);
        for (uint32_t j = 0; j < 8U; ++j) {
            int32_t a = static_cast<int32_t>((d >> (4U * j + 0U)) & 0x3U);
            int32_t b = static_cast<int32_t>((d >> (4U * j + 2U)) & 0x3U);
            int32_t c = a - b;
            if (c < 0) {
                c += static_cast<int32_t>(Q);
            }
            c %= static_cast<int32_t>(Q);
            dst_row[8U * i + j] = c;
        }
    }
}
#endif

#if defined(F203_CBD_ETA2_P0_SCALAR) || defined(F203_CBD_ETA2_P1A_SCALAR_IO)
/**
 * P0/P1a I/O：逐字节/逐 int32 scalar GM 访问。
 * 实测占 ~85% tick（见 CBD_ETA2_OPTIM_PLAN §12）；P1b 起改 DataCopy 路径。
 * @param prf_gm [in]  GM 指针，PRF 输出，形状 [ROWS=8, PRF_BYTES=128] uint8，行优先
 * @param src_gm [out] GM 指针，CBD 采样结果，形状 [ROWS=8, N=256] int32，行优先
 *               （前 4 行 = ŝ 各分量，后 4 行 = ê 各分量）
 * 前置条件：仅当编译期定义 F203_CBD_ETA2_P0_SCALAR 或 F203_CBD_ETA2_P1A_SCALAR_IO
 * 时启用（对照/调试路径，非默认）；本函数内逐字节把一行 PRF 从 GM 搬到栈上
 * （prf_local），计算后再逐 int32 写回 GM（无 DataCopy MTE 指令，纯标量访存）。
 */
__aicore__ inline void SamplePolyCbd2Batch8(__gm__ const uint8_t *prf_gm, __gm__ int32_t *src_gm)
{
    uint8_t prf_local[PRF_BYTES];
    for (uint32_t row = 0; row < ROWS; ++row) {
        /* 逐字节从 GM 搬一行 PRF 输出到栈缓冲（P0/P1a 对照路径的标量 I/O 特征） */
        const __gm__ uint8_t *prf_row_gm = prf_gm + row * PRF_BYTES;
        for (uint32_t b = 0; b < PRF_BYTES; ++b) {
            prf_local[b] = prf_row_gm[b];
        }
        int32_t row_local[N];
#if defined(F203_CBD_ETA2_P0_SCALAR)
        SamplePolyCbd2Row(row_local, prf_local);
#else
        SamplePolyCbd2RowSwLut(row_local, prf_local);
#endif
        /* 逐 int32 把该行计算结果写回 GM 对应行 */
        __gm__ int32_t *dst_row = src_gm + row * N;
        for (uint32_t j = 0; j < N; ++j) {
            dst_row[j] = row_local[j];
        }
    }
}
#else

/**
 * P1b/P2 默认：DataCopy + SWAR+LUT；按 GetBlockIdx() 分片（P2）。
 * @param prf_gm [in]  GM 指针，PRF 输出，形状 [ROWS=8, PRF_BYTES=128] uint8
 * @param src_gm [out] GM 指针，CBD 采样结果，形状 [ROWS=8, N=256] int32
 * 实际分片与流水线细节见 `f203_cbd_eta2_ub_io.hpp::SamplePolyCbd2Batch8DataCopy`
 * （按当前 blockIdx 决定单核串行 8 行还是双 AIV 各 4 行）。
 */
__aicore__ inline void SamplePolyCbd2Batch8(__gm__ const uint8_t *prf_gm, __gm__ int32_t *src_gm)
{
    const uint32_t blockIdx = AscendC::GetBlockIdx();
    SamplePolyCbd2Batch8DataCopy(blockIdx, prf_gm, src_gm);
}

#endif

}  // namespace F203CbdEta2
