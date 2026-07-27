/**
 * @file f203_cbd_eta2.hpp
 * @brief FIPS 203 Alg.8 SamplePolyCBD_η=2 — batch 4 poly 入口与设备侧契约（ML-KEM-512，k=2）。
 *
 * 数据流（默认路径）：
 *   prf_out[4,128] GM ──DataCopy──► UB ──load32+SWAR+CBD2 LUT──► src[4,256] GM
 *
 * 布局：T-B2 polyvec4，行 0=s0、1=s1、2=e0、3=e1；每行独立执行 CBD_2。
 * 默认 SIM/NPU 使用 AIV_ONLY + blockDim=2，行表锁定为 AIV0 `{0,2}` / AIV1 `{1,3}`，
 * 即每个 AIV 各处理一行 s 和一行 e。CPU 孪生 launch 为 1 时，block0 串行处理 4 行。
 */
#pragma once

#include "kernel_operator.h"

#include <cstdint>

namespace F203CbdEta2 {

constexpr uint32_t K = 2U;       // ML-KEM-512 模块秩 k
constexpr uint32_t N = 256U;     // 每个多项式系数个数
constexpr uint32_t Q = 3329U;    // FIPS 203 模数 q
constexpr uint32_t ETA = 2U;     // CBD 噪声参数 η2=2
constexpr uint32_t PRF_BYTES = (ETA * N) / 4U;  // 单 poly PRF 输出：128B
constexpr uint32_t ROWS = 2U * K;               // 4 行：s0,s1,e0,e1
constexpr uint32_t PRF_TOTAL_BYTES = ROWS * PRF_BYTES;
constexpr uint32_t SRC_COEFFS = ROWS * N;

constexpr uint32_t ROWS_PER_AIV = ROWS / 2U;
constexpr uint32_t CBD_BLOCK_DIM = 2U;

/**
 * P2 行表（锁定参数）：AIV0 处理 `{0,2}`，AIV1 处理 `{1,3}`。
 * 行号 0/1 为 s0/s1，2/3 为 e0/e1；这样两个 AIV 都各有一个 s 行与一个 e 行，
 * 避免在 512 T-B2 polyvec4 中引入零垫或 k3 的 6 行分片假设。
 */
constexpr uint32_t kAiv0Rows[ROWS_PER_AIV] = {0U, 2U};
constexpr uint32_t kAiv1Rows[ROWS_PER_AIV] = {1U, 3U};

/**
 * 由 blockIdx 与局部行号查表得到全局行号。
 * @param blockIdx 当前 AIV block 编号，P2 下为 0 或 1
 * @param localRow 当前 AIV 内部行号，范围 [0,2)
 * @return 全局行号，范围 [0,4)
 */
__aicore__ inline uint32_t RowForBlock(uint32_t blockIdx, uint32_t localRow)
{
    return (blockIdx == 0U) ? kAiv0Rows[localRow] : kAiv1Rows[localRow];
}

/**
 * 标量路径小端读取 4 字节，组成 FIPS 203 CBD_2 的 32-bit 工作字。
 * @param buf 指向至少 4 字节的 PRF 行缓冲区
 * @return 小端拼装的 32-bit 值
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
 * P0 对照：逐 32-bit 字、逐系数标量计算 CBD_2。
 * @param dst_row [out] 单行输出，长度 N=256 的 int32 数组
 * @param prf_row [in]  单行 PRF 输出，长度 PRF_BYTES=128 的 uint8 数组
 *
 * 算法对齐 FIPS 203 SamplePolyCBD_2：每 4 字节加载为 t，用
 * `0x55555555` 分别累加 bit0/bit1，把每 4 bit 形成一组 (a,b)；
 * a 取低 2 bit，b 取高 2 bit，输出 (a-b) mod q。该路径保留分支与 `%Q`，
 * 仅作调试对照，默认路径使用 LUT。
 */
__aicore__ inline void SamplePolyCbd2Row(int32_t *dst_row, const uint8_t *prf_row)
{
    for (uint32_t i = 0; i < N / 8U; ++i) {
        const uint32_t t = Load32Le(prf_row + 4U * i);
        const uint32_t d = (t & 0x55555555U) + ((t >> 1) & 0x55555555U);
        for (uint32_t j = 0; j < 8U; ++j) {
            const int32_t a = static_cast<int32_t>((d >> (4U * j)) & 0x3U);
            const int32_t b = static_cast<int32_t>((d >> (4U * j + 2U)) & 0x3U);
            int32_t c = a - b;
            if (c < 0) {
                c += static_cast<int32_t>(Q);
            }
            dst_row[8U * i + j] = c % static_cast<int32_t>(Q);
        }
    }
}
#endif

#if defined(F203_CBD_ETA2_P0_SCALAR) || defined(F203_CBD_ETA2_P1A_SCALAR_IO)
/**
 * P0/P1a I/O：逐字节从 GM 读 PRF、逐 int32 写回 GM。
 * @param prf_gm [in]  GM 指针，形状 [ROWS=4, PRF_BYTES=128] uint8
 * @param src_gm [out] GM 指针，形状 [ROWS=4, N=256] int32
 *
 * 此路径用于对照：P0 调 `SamplePolyCbd2Row`，P1a 调 SWAR+LUT 版
 * `SamplePolyCbd2RowSwLut`；两者都保留 scalar GM I/O，不作为默认验收路径。
 */
__aicore__ inline void SamplePolyCbd2Batch4(__gm__ const uint8_t *prf_gm, __gm__ int32_t *src_gm)
{
    uint8_t prf_local[PRF_BYTES];
    for (uint32_t row = 0; row < ROWS; ++row) {
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
        __gm__ int32_t *dst_row = src_gm + row * N;
        for (uint32_t j = 0; j < N; ++j) {
            dst_row[j] = row_local[j];
        }
    }
}
#else

/**
 * 默认入口：DataCopy + SWAR+LUT；按 blockIdx 走单核串行或双 AIV 分片。
 * @param prf_gm [in]  GM 指针，形状 [4,128] uint8
 * @param src_gm [out] GM 指针，形状 [4,256] int32
 */
__aicore__ inline void SamplePolyCbd2Batch4(__gm__ const uint8_t *prf_gm, __gm__ int32_t *src_gm)
{
    const uint32_t blockIdx = AscendC::GetBlockIdx();
    SamplePolyCbd2Batch4DataCopy(blockIdx, prf_gm, src_gm);
}

#endif

}  // namespace F203CbdEta2
