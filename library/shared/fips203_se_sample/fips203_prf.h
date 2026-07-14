/**
 * @file fips203_prf.h
 * @brief FIPS 203 PRF_η 抽象接口；后端可替换（标量 SHAKE128/256 → 设备批量 SHAKE256）。
 * @see ascendc-tests/fix-f203-alg13-host-scalar-fullchain-k4/DEVICE_PRF_BATCH_PLAN.md
 */
#ifndef FIPS203_PRF_H
#define FIPS203_PRF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** PRF_η(σ, N) → outlen 字节。外部只依赖此接口；内部后端由 fips203_prf.c 选择。 */
void fips203_prf(uint8_t *out, uint32_t outlen, const uint8_t sigma[32], uint8_t nonce);

/** 标量 SHAKE128-shim（tiny_sha3）；bring-up / ops-math 对拍轨 */
void fips203_prf_shake128_scalar(uint8_t *out, uint32_t outlen, const uint8_t sigma[32], uint8_t nonce);

/** 标量 SHAKE256（tiny_sha3）；FIPS 规范轨 */
void fips203_prf_shake256_scalar(uint8_t *out, uint32_t outlen, const uint8_t sigma[32], uint8_t nonce);

#ifdef __cplusplus
}
#endif

#endif
