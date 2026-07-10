/**
 * @file f203_cbd_eta2_config.h
 * @brief Host 侧布局常量（不含 kernel_operator.h，main.cpp 可独立 include）。
 *
 * 本文件在流水线中的位置：main.cpp（Host）用于计算 input/output 文件大小、
 * 校验 tiling 尺寸等，不参与设备侧编译单元。数值须与设备侧
 * `f203_cbd_eta2.hpp::F203CbdEta2` 命名空间下同名常量保持一致（两者独立定义，
 * 因为设备头依赖 kernel_operator.h，Host 侧不能直接 include）。
 */
#pragma once

#include <cstdint>

namespace F203CbdEta2Host {

constexpr uint32_t K = 4U;                              // ML-KEM-1024 模块秩 k
constexpr uint32_t N = 256U;                             // 每个多项式的系数个数
constexpr uint32_t ETA = 2U;                             // FIPS 203 CBD 噪声参数 η（本探针固定 η=2）
constexpr uint32_t PRF_BYTES = (ETA * N) / 4U;           // 单行 PRF 输出字节数：η·N/4 = 128
constexpr uint32_t ROWS = 2U * K;                        // 8 行：前 K 行对应 ŝ，后 K 行对应 ê（batch 语义标注）
constexpr uint32_t PRF_TOTAL_BYTES = ROWS * PRF_BYTES;   // 全部 8 行 PRF 输出总字节数：8*128=1024
constexpr uint32_t SRC_COEFFS = ROWS * N;                // 输出 src 的系数总数：8*256=2048
constexpr uint32_t CBD_BLOCK_DIM = 2U;                   // 探针默认 blockDim（P2 双 AIV，SIM/NPU 最优）

}  // namespace F203CbdEta2Host

/* F203_CBD_BLOCK_DIM 由 CMake -Dcbd_block_dim=1|2 或测试 override
 * CBD_TEST_BLOCK_DIM 环境变量传入；未定义时默认 2（P2 双 AIV，探针默认最优路径）。 */
#if !defined(F203_CBD_BLOCK_DIM)
#define F203_CBD_BLOCK_DIM 2
#endif

/* kHostBlockDim：Host 侧实际下发核函数时使用的 blockDim，须与设备侧编译期
 * F203_CBD_BLOCK_DIM 分支保持一致（=1 时单 AIV 串行 8 行，其余走双 AIV 分片）。 */
#if F203_CBD_BLOCK_DIM == 1
constexpr uint32_t kHostBlockDim = 1U;
#else
constexpr uint32_t kHostBlockDim = F203CbdEta2Host::CBD_BLOCK_DIM;
#endif
