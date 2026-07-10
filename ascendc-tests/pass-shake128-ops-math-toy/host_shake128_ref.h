/**
 * @file host_shake128_ref.h
 * @brief Host 参考：tiny_sha3 标量 SHAKE128（与 Python hashlib.shake_128 对拍）
 *
 * 本文件在流水线中的位置：作为 `pass-shake128-ops-math-toy` 探针的 **Host 侧参考实现**
 * 声明头，被 `verify_result.py` 编译为动态库（`host_shake128_ref.c` + 第三方
 * `thirdparty/tiny_sha3/sha3.c`）后通过 ctypes 调用，用于与设备（AscendC UB 内自检）
 * 及 Python `hashlib.shake_128` 三方对拍，作为 SHAKE128 语义正确性的黑盒 oracle 之一。
 * 不参与设备侧编译，也不是 AscendC 实现规格——仅提供合法输入下的期望输出。
 */
#ifndef HOST_SHAKE128_REF_H
#define HOST_SHAKE128_REF_H

#include <stdint.h>

/**
 * 对单条消息计算 SHAKE128 输出（标量 tiny_sha3 实现）。
 * @param out    [out] 输出缓冲区，调用者分配，长度须 >= outlen 字节
 * @param outlen [in]  期望输出字节数（SHAKE 为可扩展输出函数，任意长度均合法）
 * @param msg    [in]  输入消息指针；msg_len 为 0 时可为任意值（函数内部不会解引用）
 * @param msg_len [in] 输入消息字节长度；允许为 0（空消息合法输入）
 */
void host_shake128_one(uint8_t *out, uint32_t outlen, const uint8_t *msg, uint32_t msg_len);

/**
 * 对一批消息逐条计算 SHAKE128（batch 语义与设备 `shake_general` 核对齐，便于逐条对拍）。
 * @param y         [out] 输出缓冲区，行优先布局 [batch, outLen]，每行为该条消息的 SHAKE128 输出
 * @param x         [in]  输入缓冲区，行优先布局 [batch, maxMsgLen]（每条消息按 maxMsgLen 补齐存放，
 *                        实际有效长度由 lengths[i] 给出，超出部分不参与哈希）
 * @param lengths   [in]  每条消息的真实字节长度数组，长度为 batch
 * @param batch     [in]  消息条数
 * @param maxMsgLen [in]  x 中每条消息占用的最大字节数（用于按行取出真实消息切片）
 * @param outLen    [in]  每条消息的期望输出字节数（本 batch 内所有消息统一 outLen）
 */
void host_shake128_batch(uint8_t *y, const uint8_t *x, const uint32_t *lengths, uint32_t batch, uint32_t maxMsgLen,
                         uint32_t outLen);

#endif
