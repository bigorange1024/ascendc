/**
 * @file host_j_shake.hpp
 * @brief Host 侧 J = SHAKE256(z‖c, 32)（tiny_sha3；Alg.21 拒绝路径）。
 *
 * 背景：FO 在 Host 完成；正确性优先，非常量时间。
 * 未采用设备 FO 核；禁抄 alg21 设备 FO 实现。
 */
#pragma once

#include <cstdint>
#include <cstring>

extern "C" {
#include "sha3.h"
}

namespace host_j {

/** J(z‖c)：SHAKE256(z‖c, 32B)。@return true 成功 */
inline bool Shake256J(uint8_t *kOut, const uint8_t *z, const uint8_t *c, size_t cLen)
{
    uint8_t msg[32 + 1568];
    if (cLen > 1568) {
        return false;
    }
    std::memcpy(msg, z, 32);
    std::memcpy(msg + 32, c, cLen);
    sha3_ctx_t ctx;
    shake256_init(&ctx);
    shake_update(&ctx, msg, 32U + static_cast<unsigned int>(cLen));
    shake_xof(&ctx);
    shake_out(&ctx, kOut, 32);
    return true;
}

} // namespace host_j
