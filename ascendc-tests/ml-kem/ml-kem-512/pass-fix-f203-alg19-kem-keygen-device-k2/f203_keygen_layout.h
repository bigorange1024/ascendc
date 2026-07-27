// @probe pass-fix-f203-alg13-device-keygen-k2
// @file f203_keygen_layout.h
// @layer host
// @role 头文件/内联：`f203_keygen_layout.h` 声明或配置 AscendC/host 接口与常量。 / Header `f203_keygen_layout.h`.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch N/A（host / 脚本 / CMake 不参与 device launch）
// @ai_core N/A（非 AI Core 内核源）
// @depends #include: cstdint
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。


/**
 * @file f203_keygen_layout.h
 * @brief Alg.13 KeyGen 行 21 公钥/私钥字节尺寸常量（Host 与 compute 共用语义）。
 *
 * ## 流水线位置
 * 定义 ek_PKE = ByteEncode₁₂(t̂) ‖ ρ、dk_PKE = ByteEncode₁₂(ŝ) 的定长；
 * `main_keygen.cpp` 分配/写盘与 `FuseEkPke` 拼接均依赖本头。
 *
 * ## 对齐
 * FIPS 203 ML-KEM-512（k=2）：每 poly 12-bit 编码 384B；k 个 poly → 768B；
 * ek 再附 ρ 32B → 800B。与 golden I/O 尺寸一致（仅验字节长度与内容等价）。
 */
#pragma once

#include <cstdint>

namespace F203Keygen {

/** 矩阵维数 k（ML-KEM-512） */
constexpr uint32_t kKyberK = 2U;
/** ByteEncode₁₂ 单 poly 字节数：256×12/8 = 384 */
constexpr uint32_t kPolyBytesEncoded = 384U;
/** ek/dk 中 polyvec 编码长度：k×384 = 768 */
constexpr uint32_t kEkPolyvecBytes = kKyberK * kPolyBytesEncoded;  // 768
/** 公钥种子 ρ 长度（SHA3-512(G) 前半） */
constexpr uint32_t kRhoBytes = 32U;
/** 生产公钥 ek_PKE = ek_polyvec ‖ ρ */
constexpr uint32_t kEkPkeBytes = kEkPolyvecBytes + kRhoBytes;      // 800
/** 生产私钥 dk_PKE = sk_polyvec（本交付不含 H(ek)‖z 等 KEM 扩展） */
constexpr uint32_t kDkPkeBytes = kEkPolyvecBytes;                  // 768

}  // namespace F203Keygen
