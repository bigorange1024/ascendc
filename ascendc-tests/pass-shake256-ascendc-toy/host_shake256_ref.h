/**
 * @file host_shake256_ref.h
 * @brief Host 参考：tiny_sha3 标量 SHAKE256（FIPS PRF 规范轨），与 Python
 * hashlib.shake_256 对拍。
 *
 * 本文件在流水线中的位置：`pass-shake256-ascendc-toy` 探针的 Host 侧参考实现声明头，
 * 被 `verify_result.py` 编译为动态库（`host_shake256_ref.c` + 第三方
 * `thirdparty/tiny_sha3/sha3.c`）后通过 ctypes 调用，作为 SHAKE256 语义正确性的
 * 黑盒 oracle 之一，与设备侧 UB 自检、Python `hashlib.shake_256` 三方对拍。
 * SHAKE256 是 FIPS 203 ML-KEM 中 PRF/H/G/J 等函数的规范轨基础原语（与
 * `pass-shake128-ops-math-toy` 的 SHAKE128 shim 轨不同），不参与设备侧编译，
 * 也不是 AscendC 实现规格——仅提供合法输入下的期望输出。
 */
#pragma once

#include <stdint.h>

/**
 * 对单条消息计算 SHAKE256 输出（标量 tiny_sha3 实现）。
 * @param out    [out] 输出缓冲区，调用者分配，长度须 >= outlen 字节
 * @param outlen [in]  期望输出字节数（SHAKE 为可扩展输出函数，任意长度均合法）
 * @param msg    [in]  输入消息指针；msg_len 为 0 时可为任意值
 * @param msg_len [in] 输入消息字节长度；允许为 0（空消息合法输入）
 */
void host_shake256_one(uint8_t *out, uint32_t outlen, const uint8_t *msg, uint32_t msg_len);

/**
 * 对一批消息逐条计算 SHAKE256（batch 语义与设备 `shake_general` 核对齐，便于逐条对拍）。
 * @param y         [out] 输出缓冲区，行优先布局 [batch, outLen]
 * @param x         [in]  输入缓冲区，行优先布局 [batch, maxMsgLen]（真实长度见 lengths）
 * @param lengths   [in]  每条消息的真实字节长度数组，长度为 batch
 * @param batch     [in]  消息条数
 * @param maxMsgLen [in]  x 中每条消息占用的最大字节数
 * @param outLen    [in]  每条消息的期望输出字节数
 */
void host_shake256_batch(uint8_t *y, const uint8_t *x, const uint32_t *lengths, uint32_t batch, uint32_t maxMsgLen,
                         uint32_t outLen);
