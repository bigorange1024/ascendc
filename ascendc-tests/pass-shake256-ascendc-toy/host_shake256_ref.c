/**
 * @file host_shake256_ref.c
 * @brief host_shake256_ref.h 的实现：调用第三方 tiny_sha3（thirdparty/tiny_sha3/sha3.c）
 * 完成标量 SHAKE256 计算。仅作 Host 侧对拍参考，不下发设备，不是 AscendC 实现规格。
 */
#include "host_shake256_ref.h"

#include <string.h>

#include "sha3.h"

/**
 * 单条消息 SHAKE256：tiny_sha3 三段式调用 —— 初始化（指定 rate=SHAKE256，即
 * capacity=512bit/rate=136B）→ 吸收（仅在 msg_len>0 时调用）→ 切换到挤出模式（XOF）→
 * 按 outlen 挤出任意长度输出。
 */
void host_shake256_one(uint8_t *out, uint32_t outlen, const uint8_t *msg, uint32_t msg_len)
{
    sha3_ctx_t ctx;
    shake256_init(&ctx);
    if (msg_len > 0U) {
        shake_update(&ctx, msg, msg_len);
    }
    shake_xof(&ctx);
    shake_out(&ctx, out, outlen);
}

/**
 * batch 条消息逐条调用 host_shake256_one；x/y 均按固定跨距（maxMsgLen / outLen）
 * 行优先排布，与设备 shake_general 核的 batch 语义一致，便于逐行对拍。
 */
void host_shake256_batch(uint8_t *y, const uint8_t *x, const uint32_t *lengths, uint32_t batch, uint32_t maxMsgLen,
                         uint32_t outLen)
{
    for (uint32_t i = 0; i < batch; ++i) {
        /* 第 i 条消息在 x 中的起始位置：按 maxMsgLen 定跨距，真实长度以 lengths[i] 为准 */
        const uint8_t *msg = x + i * maxMsgLen;
        /* 第 i 条消息输出写入位置：按统一 outLen 定跨距 */
        uint8_t *out = y + i * outLen;
        host_shake256_one(out, outLen, msg, lengths[i]);
    }
}
