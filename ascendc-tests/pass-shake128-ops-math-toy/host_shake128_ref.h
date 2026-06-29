/**
 * @file host_shake128_ref.h
 * @brief Host 参考：tiny_sha3 标量 SHAKE128（与 Python hashlib.shake_128 对拍）
 */
#ifndef HOST_SHAKE128_REF_H
#define HOST_SHAKE128_REF_H

#include <stdint.h>

/** 单条消息 SHAKE128；msg_len 可为 0 */
void host_shake128_one(uint8_t *out, uint32_t outlen, const uint8_t *msg, uint32_t msg_len);

/** batch 条消息；x 布局 [batch,maxMsgLen]，lengths[batch]，y [batch,outLen] */
void host_shake128_batch(uint8_t *y, const uint8_t *x, const uint32_t *lengths, uint32_t batch, uint32_t maxMsgLen,
                         uint32_t outLen);

#endif
