/**
 * @file decompress_d1_ref.h
 * @brief Decompress_1 消息嵌入 host 参考 API（golden）。
 */
#ifndef DECOMPRESS_D1_REF_H
#define DECOMPRESS_D1_REF_H

#include <stdint.h>

/** v ← v + Decompress_1(m) (mod q)；in/out 长度 n=256。 */
void embed_message_ref(int32_t *out, const int32_t *in, const uint8_t *m, int32_t n);

/** 仅输出 μ_embed[256]；不写 in。 */
void mu_embed_only_ref(int32_t *mu_out, const uint8_t *m, int32_t n);

#endif
