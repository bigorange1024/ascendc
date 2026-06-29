/**
 * @file integration_config.hpp
 * @brief v2 探针行 18–20 集成开关：Alg11 向量 basemul、dot-only、全 poly、ByteEncode、探测与 UB 尺寸常量。
 *
 * 用途：编译期门禁，决定 `2s1e_post_ntt_ub.hpp` / mmad_custom 链接的行 18 形态与 mixPass 可运行阶段。
 *
 * 调用方：pipeline_probe.hpp、hat_alg11_basemul.hpp、`2s1e_post_ntt_ub.hpp`、CMake 经 -D 注入内核。
 *
 * 不变量（v2 生产默认，run.sh 全链路）：
 *   - HAT_ALG11_VEC=1、HAT_LINE18_FULLPOLY=1、HAT_LINE18_DOT_ONLY=0（Σ+ê+一次 mod）；
 *   - HAT_BYTE_ENCODE=1（行 19–20 ByteEncode₁₂）；
 *   - hat_alg11_cfg::kExtraInt32Slots = ROM + basemul ws + γ 切片。
 *
 * Golden：
 *   - DOT_ONLY=1 → golden_t_hat_dot.bin；
 *   - DOT_ONLY=0 → golden_t_hat_c.bin + ek/sk（HAT_BYTE_ENCODE=1）。
 *
 * CMake（CMakeLists.txt CACHE → cpu_lib/npu_lib / ascendc_kernels_bbit）：
 *   HAT_ALG11_VEC、HAT_LINE18_DOT_ONLY、HAT_BYTE_ENCODE、F203_PIPELINE_PROBE、
 *   BYTE_ENCODE12_VEC、BYTE_ENCODE12_SCATTER_VEC、ALG11_IMPL/VEC_VARIANT/VEC_OPTS/MEM_OPS。
 *   HAT_LINE18_FULLPOLY 当前仅头文件默认（未编入 CMake，改宏后重编）。
 */
#ifndef INTEGRATION_CONFIG_HPP
#define INTEGRATION_CONFIG_HPP

#include "multiply_ntts_config.hpp"

/** 行 18：1=Alg11 向量 basemul（B2+MEM_OPS=1）；0=标量 multiply_ntts_half_scalar */
#ifndef HAT_ALG11_VEC
#define HAT_ALG11_VEC 1
#endif

/**
 * 1=仅 Â·ŝ dot（无 ê、无 ByteEncode），对拍 golden_t_hat_dot.bin。
 * 0=dot + ê（+ 可选 ByteEncode，见 HAT_BYTE_ENCODE）。
 */
#ifndef HAT_LINE18_DOT_ONLY
#define HAT_LINE18_DOT_ONLY 1
#endif

/** v2：行 18 用全 poly j→p compute_on_ub（非 legacy half 路径）。 */
#ifndef HAT_LINE18_FULLPOLY
#define HAT_LINE18_FULLPOLY 1
#endif

/**
 * 1=行 19–20 ByteEncode（须 HAT_LINE18_DOT_ONLY=0）。
 * 阶段 A 置 0；阶段 B 置 1。
 */
#ifndef HAT_BYTE_ENCODE
#define HAT_BYTE_ENCODE 0
#endif

/**
 * 开发期中间态探测：1=CPU printf 抽样前 N 个系数（见 pipeline_probe.hpp）。
 * 测性能/SIM 发布对比时保持 0。
 */
#ifndef F203_PIPELINE_PROBE
#define F203_PIPELINE_PROBE 0
#endif

#ifndef BYTE_ENCODE12_VEC
#define BYTE_ENCODE12_VEC 1
#endif

#ifndef BYTE_ENCODE12_SCATTER_VEC
#define BYTE_ENCODE12_SCATTER_VEC 1
#endif

namespace hat_alg11_cfg {
constexpr uint32_t kRomInt32Slots = 4U * 128U;
constexpr uint32_t kBasemulWsInts = 8U * 128U;
constexpr uint32_t kGammaSliceInts = 128U;
constexpr uint32_t kExtraInt32Slots = kRomInt32Slots + kBasemulWsInts + kGammaSliceInts;
constexpr uint32_t kHatPcMult = 8U;
} // namespace hat_alg11_cfg

#endif
