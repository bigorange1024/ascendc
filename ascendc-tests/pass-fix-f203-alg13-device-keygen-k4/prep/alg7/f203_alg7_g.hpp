// @probe pass-fix-f203-alg13-device-keygen-k4
// @file prep/alg7/f203_alg7_g.hpp
// @layer prep
// @role prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；含 XOF、rej 标量/向量与 compact LUT ROM。 / Alg.7 SampleNTT rejection-sampling prep kernels. 本文件 `f203_alg7_g.hpp` 为该子模块组件。 / Component: f203_alg7_g.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影，以 profile_subtask_log*.toml 为准。
// @depends #include: f203_alg7_layout.h, f203_se_device_keccak.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_alg7_g.hpp
 * @brief Alg.7 输入派生：SEED_D → d → ρ（与 presample Phase G 同式）。
 *
 * 流水线位置：BuildAlg7SampleNttFromSeedD 链首，在 SHAKE 之前于 UB 外栈上完成；
 * 产出 ρ[32] 供 FillSampleSeedUb 拼接 byte(j)||byte(i)。
 *
 * 数学契约：
 *   - DerandFromSeedD：SHA3-256("exp-mlkem-f203-2s1e-k4:SEED_D=<decimal>") → d[32]
 *   - HashGRho：SHA3-512(d || byte(k=4)) 取前 32B → ρ
 *   - HashGSigma：同一次 SHA3-512 取后 32B → σ（见 BuildRhoSigmaFromSeedD）
 *
 * 与 golden 关系：gen_data.py 须使用相同域分离串与 k=4 后缀；ρ 不一致则 XOF 全链偏离。
 */
#pragma once

#include "f203_alg7_layout.h"
#include "f203_se_device_keccak.hpp"

namespace F203Alg7 {

/**
 * 无除法 uint32 → 十进制 ASCII（设备侧无 printf/snprintf）。
 * @param v   待转换无符号整数（SEED_D）
 * @param out 输出缓冲，调用方保证足够长（最大 10 位+'\0' 语义）
 * @return    写入字符个数（不含 NUL）
 */
__aicore__ inline int U32ToDec(uint32_t v, char *out)
{
    char tmp[10];
    int n = 0;
    if (v == 0U) {
        tmp[n++] = '0';
    } else {
        while (v > 0U) {
            tmp[n++] = static_cast<char>('0' + (v % 10U));
            v /= 10U;
        }
    }
    // 反转得到高位在前
    for (int i = 0; i < n; ++i) {
        out[i] = tmp[n - 1 - i];
    }
    return n;
}

/**
 * SEED_D → 域分离消息 → SHA3-256 → d[32]。
 * 消息格式固定为仓库探针前缀 + 十进制 SEED_D（见 U32ToDec）。
 */
__aicore__ inline void DerandFromSeedD(uint32_t seed_d, uint8_t d[32])
{
    char msg[48];
    int pos = 0;
    // 固定前缀 "exp-mlkem-f203-2s1e-k4:SEED_D="
    msg[pos++] = 'e';
    msg[pos++] = 'x';
    msg[pos++] = 'p';
    msg[pos++] = '-';
    msg[pos++] = 'm';
    msg[pos++] = 'l';
    msg[pos++] = 'k';
    msg[pos++] = 'e';
    msg[pos++] = 'm';
    msg[pos++] = '-';
    msg[pos++] = 'f';
    msg[pos++] = '2';
    msg[pos++] = '0';
    msg[pos++] = '3';
    msg[pos++] = '-';
    msg[pos++] = '2';
    msg[pos++] = 's';
    msg[pos++] = '1';
    msg[pos++] = 'e';
    msg[pos++] = '-';
    msg[pos++] = 'k';
    msg[pos++] = '4';
    msg[pos++] = ':';
    msg[pos++] = 'S';
    msg[pos++] = 'E';
    msg[pos++] = 'E';
    msg[pos++] = 'D';
    msg[pos++] = '_';
    msg[pos++] = 'D';
    msg[pos++] = '=';
    pos += U32ToDec(seed_d, msg + pos);
    F203SeDeviceKeccak::Sha3OneShot(d, 32, reinterpret_cast<const uint8_t *>(msg), static_cast<uint32_t>(pos));
}

/**
 * G(d) → gout[64]：SHA3-512(d || byte(k)) 一次 squeeze（ρ‖σ 同源）。
 */
__aicore__ inline void HashGFull(const uint8_t d[32], uint8_t gout[64])
{
    uint8_t in[33];
    for (int i = 0; i < 32; ++i) {
        in[i] = d[i];
    }
    in[32] = static_cast<uint8_t>(kKyberK & 0xFFU);
    F203SeDeviceKeccak::Sha3OneShot(gout, 64, in, sizeof(in));
}

/**
 * G(d) → ρ[32]：SHA3-512(d || byte(k)) 输出截断前半。
 * k 取 kKyberK=4，与 ML-KEM 参数集一致。
 */
__aicore__ inline void HashGRho(const uint8_t d[32], uint8_t rho[32])
{
    uint8_t out[64];
    HashGFull(d, out);
    for (int i = 0; i < 32; ++i) {
        rho[i] = out[i];
    }
}

/** G(d) → σ[32]：同一次 SHA3-512 输出的后半。 */
__aicore__ inline void HashGSigma(const uint8_t d[32], uint8_t sigma[32])
{
    uint8_t out[64];
    HashGFull(d, out);
    for (int i = 0; i < 32; ++i) {
        sigma[i] = out[32 + i];
    }
}

/**
 * SEED_D → 一次 Derand + SHA3-512 → ρ‖σ。
 * KeyGen 单 TPipe / 子探针对齐路径：避免 Â 与 presample 各算一遍 G。
 */
__aicore__ inline void BuildRhoSigmaFromSeedD(uint32_t seed_d, uint8_t rho[32], uint8_t sigma[32])
{
    uint8_t d[32];
    DerandFromSeedD(seed_d, d);
    uint8_t gout[64];
    HashGFull(d, gout);
    for (int i = 0; i < 32; ++i) {
        rho[i] = gout[i];
        sigma[i] = gout[32 + i];
    }
}

/** 组合：SEED_D → d → ρ，供 SHAKE 采样种子构造。 */
__aicore__ inline void BuildRhoFromSeedD(uint32_t seed_d, uint8_t rho[32])
{
    uint8_t sigma_unused[32];
    BuildRhoSigmaFromSeedD(seed_d, rho, sigma_unused);
}

}  // namespace F203Alg7
