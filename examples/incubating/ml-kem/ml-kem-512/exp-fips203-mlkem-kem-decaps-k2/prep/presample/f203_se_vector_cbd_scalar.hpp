// @probe exp-fips203-mlkem-kem-decaps-k2
// @file prep/presample/f203_se_vector_cbd_scalar.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样
// @production_io Encrypt prep：input ek_pke.bin+coins.bin；output a_hat.bin+re.bin；中间 prf 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY
// @ai_core SIM：0×AIC+2×AIV；双 AIV 并行 Â；block0 独占 PRF+CBD。
// @depends 见文件内 #include
// @verify run.sh CPU+SIM；verify_result.py max_abs_diff=0。


/**
 * @file f203_se_vector_cbd_scalar.hpp
 * @brief V3 Phase C：标量 SamplePolyCBD_η=2（与 se-device-scalar 同式，生产默认对照路径）。
 *
 * 流水线位置（presample / KeyGen 行 8–15 对照）：
 *   prf_out[8,128] → 逐行标量 CBD → src[8,256] int32
 * Encrypt prep 生产路径用 alg8 SWAR+LUT（f203_encrypt_re_cbd.hpp），本文件保留作标量语义参考。
 *
 * 与 golden：sample_poly_cbd2（FIPS 203 / Kyber）。
 */
#pragma once

#include "kernel_operator.h"

#include <cstdint>

namespace F203SeVector {

constexpr uint32_t N = 256U;
constexpr uint32_t Q = 3329U;
/** KeyGen/presample：8 行（s‖e），不含 Encrypt 的 e₂。 */
constexpr uint32_t SRC_ROWS = 8U;

/** 小端 load32：从 PRF 缓冲取 4B。 */
__aicore__ inline uint32_t Load32Le(const uint8_t *buf)
{
    return static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
}

/**
 * 单行 CBD_η=2：prf_row[128] → dst_row[256]。
 *
 * SWAR：d 的每 4 bit 含 (a,b)；c = (a-b) mod q。
 */
__aicore__ inline void SamplePolyCbd2Row(int32_t *dst_row, const uint8_t *buf)
{
    for (uint32_t i = 0; i < N / 8U; ++i) {
        const uint32_t t = Load32Le(buf + 4U * i);
        // 相邻 bit 对求和 → 8 组 2-bit 半系数
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

/**
 * 8 行：逐 nonce 从 prf_out GM 标量读 → CBD → 写 src GM。
 * @param prf_out_gm [8,128] uint8；@param src_gm [8,256] int32
 */
__aicore__ inline void BuildSrcFromPrfGm(const __gm__ uint8_t *prf_out_gm, __gm__ int32_t *src_gm)
{
    uint8_t prf_buf[128];
    int32_t row[N];
    for (uint8_t nonce = 0; nonce < SRC_ROWS; ++nonce) {
        // 标量 GM 读一行 PRF（慢路径；生产用 DataCopy+LUT）
        const __gm__ uint8_t *prf_row = prf_out_gm + static_cast<uint32_t>(nonce) * 128U;
        for (uint32_t i = 0; i < 128U; ++i) {
            prf_buf[i] = prf_row[i];
        }
        SamplePolyCbd2Row(row, prf_buf);
        const uint32_t base = static_cast<uint32_t>(nonce) * N;
        for (uint32_t i = 0; i < N; ++i) {
            src_gm[base + i] = row[i];
        }
    }
}

}  // namespace F203SeVector
