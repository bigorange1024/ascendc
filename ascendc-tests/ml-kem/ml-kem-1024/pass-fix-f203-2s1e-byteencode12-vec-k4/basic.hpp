/**
 * @file basic.hpp
 * @brief ByteEncode₁₂ 探针编译期便利头：引入 kernel_operator.h，
 *        并把 AscendC::LocalTensor 提到当前翻译单元常用名。
 *        不含算法逻辑。背景：Cloud 干净树缺本文件会导致 `#include "basic.hpp"` 失败
 *        （曾仅留在未跟踪/本地残留中）。
 */
#ifndef PASS_BYTEENCODE12_BASIC_HPP
#define PASS_BYTEENCODE12_BASIC_HPP

#include "kernel_operator.h"

using AscendC::LocalTensor;

#endif
