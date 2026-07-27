/**
 * @file f203_cbd_eta3_config.h
 * @brief Host 侧布局常量（ML-KEM-512 Alg.8 CBD η=3，k=2）。
 *
 * 本文件只给 `main.cpp` 使用：计算 input/output 文件大小、打印探针形状与确定
 * Host 下发 blockDim。设备侧同名语义常量在 `f203_cbd_eta3.hpp` 内独立定义，
 * 因为设备头依赖 `kernel_operator.h`，Host 侧不能直接 include。两边常量必须
 * 同步：k=2，N=256，q=3329，ROWS=4（s0,s1,e0,e1），单 poly PRF=192B。
 */
#pragma once

#include <cstdint>

namespace F203CbdEta3Host {

constexpr uint32_t K = 2U;                              // ML-KEM-512 模块秩 k
constexpr uint32_t N = 256U;                            // 每个多项式的系数个数
constexpr uint32_t ETA = 3U;                            // CBD 噪声参数 η1=3
constexpr uint32_t PRF_BYTES = (ETA * N) / 4U;          // 单行 PRF 输出字节数：3*256/4=192
constexpr uint32_t ROWS = 2U * K;                       // 4 行：s0,s1,e0,e1（T-B2 polyvec4）
constexpr uint32_t PRF_TOTAL_BYTES = ROWS * PRF_BYTES;  // 全部 4 行 PRF 输出总字节数：4*192=768
constexpr uint32_t SRC_COEFFS = ROWS * N;               // 输出 src 的系数总数：4*256=1024
constexpr uint32_t CBD_BLOCK_DIM = 2U;                  // 探针默认 blockDim：双 AIV

}  // namespace F203CbdEta3Host

/* F203_CBD_BLOCK_DIM 由 CMake / run.sh 的 CBD_TEST_BLOCK_DIM 传入；未定义时默认双 AIV。
 * CPU 孪生主程序仍固定 launch blockDim=1，由 kernel 内 GetBlockNum()==1 分支串行 4 行。 */
#if !defined(F203_CBD_BLOCK_DIM)
#define F203_CBD_BLOCK_DIM 2
#endif

#if F203_CBD_BLOCK_DIM == 1
constexpr uint32_t kHostBlockDim = 1U;
#else
constexpr uint32_t kHostBlockDim = F203CbdEta3Host::CBD_BLOCK_DIM;
#endif
