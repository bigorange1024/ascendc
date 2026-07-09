// @probe stable-mlkem-f203-pke-keygen-k4
// @file prep/alg8/f203_cbd_eta2.hpp
// @layer prep
// @role prep/alg8：η=2 centered binomial（secret/noise）设备采样与 LUT；与 presample/alg7 链接成 prep 链。 / Alg.8 η=2 CBD prep helpers. 本文件 `f203_cbd_eta2.hpp` 为该子模块组件。 / Component: f203_cbd_eta2.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: kernel_operator.h, cstdint, f203_cbd_eta2_sw_lut.hpp, f203_cbd_eta2_ub_io.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


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
 *   -Dcbd_block_dim=1|2         → P1b-single 单 AIV / P2 双 AIV
 *
 * 布局与 vec-k4-v2 `src.bin` 一致；双 AIV 分片见 RowForBlock / LAYOUT.md。
 */
#pragma once

#include "kernel_operator.h"

#include <cstdint>

namespace F203CbdEta2 {

constexpr uint32_t K = 4U;
constexpr uint32_t N = 256U;
constexpr uint32_t Q = 3329U;
constexpr uint32_t ETA = 2U;
/** PRF 输出字节 / poly：η·N/4 = 128。 */
constexpr uint32_t PRF_BYTES = (ETA * N) / 4U;
/** 8 行：前 K 行 s，后 K 行 e（语义标注；CBD 各行独立）。 */
constexpr uint32_t ROWS = 2U * K;
constexpr uint32_t PRF_TOTAL_BYTES = ROWS * PRF_BYTES;
constexpr uint32_t SRC_COEFFS = ROWS * N;

/** P2：每 AIV 处理行数。 */
constexpr uint32_t ROWS_PER_AIV = ROWS / 2U;
constexpr uint32_t CBD_BLOCK_DIM = 2U;

/**
 * P2 行表 — 与 vec-k4-v2 Stage1（Aiv2s1eSplit）一致：
 *   AIV0 → ŝ 两行 + ê 前半；AIV1 → ŝ 两行 + ê 后半。
 */
constexpr uint32_t kAiv0Rows[ROWS_PER_AIV] = {0U, 1U, 4U, 5U};
constexpr uint32_t kAiv1Rows[ROWS_PER_AIV] = {2U, 3U, 6U, 7U};

/** blockIdx 0/1 → 全局 src/prf 行号。 */
__aicore__ inline uint32_t RowForBlock(uint32_t blockIdx, uint32_t localRow)
{
    return (blockIdx == 0U) ? kAiv0Rows[localRow] : kAiv1Rows[localRow];
}

/** 栈/标量路径用 little-endian uint32 加载（P0/P1a）。 */
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
 */
__aicore__ inline void SamplePolyCbd2Batch8(__gm__ const uint8_t *prf_gm, __gm__ int32_t *src_gm)
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

/** P1b/P2 默认：DataCopy + SWAR+LUT；按 GetBlockIdx() 分片（P2）。 */
__aicore__ inline void SamplePolyCbd2Batch8(__gm__ const uint8_t *prf_gm, __gm__ int32_t *src_gm)
{
    const uint32_t blockIdx = AscendC::GetBlockIdx();
    SamplePolyCbd2Batch8DataCopy(blockIdx, prf_gm, src_gm);
}

#endif

}  // namespace F203CbdEta2
