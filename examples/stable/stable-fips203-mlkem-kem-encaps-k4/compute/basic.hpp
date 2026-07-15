/**
 * @file basic.hpp
 * @brief Encrypt compute 公共基础头：引入 AscendC kernel_operator，并导出 LocalTensor 别名。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024（k=4）K-PKE.Encrypt 的 compute 段
 * （NTT/INTT/内积等 MIX kernel）共用的最小依赖；不参与 golden I/O。
 * 本文件无算法逻辑，仅统一 include 与 using，避免各 kernel 重复声明。
 */
#ifndef F203_BASIC_HPP
#define F203_BASIC_HPP

#include "kernel_operator.h"

/** AscendC 本地张量类型别名，供 aic_func / aiv_func 等直接使用。 */
using AscendC::LocalTensor;

#endif
