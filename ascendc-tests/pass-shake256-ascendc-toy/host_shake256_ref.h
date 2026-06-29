#pragma once

#include <stdint.h>

void host_shake256_one(uint8_t *out, uint32_t outlen, const uint8_t *msg, uint32_t msg_len);

void host_shake256_batch(uint8_t *y, const uint8_t *x, const uint32_t *lengths, uint32_t batch, uint32_t maxMsgLen,
                         uint32_t outLen);
