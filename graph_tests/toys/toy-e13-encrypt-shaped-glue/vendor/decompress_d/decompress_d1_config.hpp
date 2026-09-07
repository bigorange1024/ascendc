/**
 * @file decompress_d1_config.hpp
 * @brief E11：FIPS 203 Decompress_1 消息嵌入（μ[32B]→256 系数 {0,⌊(q+1)/2⌋}）。
 *
 * 本文件在流水线中的位置：d=1 专用；与 pass-f203-decompress-d-vec-k4 的 d∈{4,5,10,11}
 * 公式 Decompress_d(u)=(u·q+bias)>>d 不同——消息嵌入为 per-bit 展开，非压缩域解压。
 * 对齐规范：FIPS 203 Alg.14 行 20 / Encrypt 链 PrefixEmbed；golden 见 decompress_d1_ref.c。
 */
#ifndef DECOMPRESS_D1_CONFIG_HPP
#define DECOMPRESS_D1_CONFIG_HPP

#include "f203_mlkem_params.h"

/** 消息字节数（FIPS 203 ML-KEM 固定 32B）。 */
#define F203_DECOMPRESS_D1_MSG_BYTES 32

/** bit=1 时的嵌入量 ⌊(q+1)/2⌋ = 1665（q=3329）。 */
#define F203_DECOMPRESS_D1_HALF_Q ((F203_MLKEM_Q + 1) / 2)

#endif
