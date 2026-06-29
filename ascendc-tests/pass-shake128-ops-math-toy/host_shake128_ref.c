#include "host_shake128_ref.h"

#include <string.h>

#include "sha3.h"

void host_shake128_one(uint8_t *out, uint32_t outlen, const uint8_t *msg, uint32_t msg_len)
{
    sha3_ctx_t ctx;
    shake128_init(&ctx);
    if (msg_len > 0U) {
        shake_update(&ctx, msg, msg_len);
    }
    shake_xof(&ctx);
    shake_out(&ctx, out, outlen);
}

void host_shake128_batch(uint8_t *y, const uint8_t *x, const uint32_t *lengths, uint32_t batch, uint32_t maxMsgLen,
                         uint32_t outLen)
{
    for (uint32_t i = 0; i < batch; ++i) {
        const uint8_t *msg = x + i * maxMsgLen;
        uint8_t *out = y + i * outLen;
        host_shake128_one(out, outLen, msg, lengths[i]);
    }
}
