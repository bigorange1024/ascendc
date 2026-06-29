#ifndef TOY_MIX_S123_ENCODE_HPP
#define TOY_MIX_S123_ENCODE_HPP

/**
 * @file toy_encode.hpp
 * @brief Stage3 末尾的「编码」玩具：对应真流程里的 ByteEncode₁₂，此处简化为 x % 64 → int8。
 *
 * 约束（与 TOY_MIX_S123.md 一致）：
 *   - 输入、输出张量均已在 UB（LocalTensor）中，禁止在此函数内访问 GM。
 *   - 真 ByteEncode 会按 12-bit 打包；本探针只验证「S3 结果在 UB 内完成标量后处理再写 out」。
 */

#include "kernel_operator.h"

namespace toy {

/**
 * @brief func1：对每个 int32 元素取模 64，再截断为 int8 写入 dst。
 *
 * @param dst  UB 输出，长度 len，dtype int8
 * @param src  UB 输入，长度 len，dtype int32（通常为 Adds(+1) 之后）
 * @param len  元素个数（每 AIV 为 kOutPerAiv = 2048）
 *
 * Golden（Python）：
 *   out = ((C.astype(np.int32) + 1) % 64).astype(np.int8)
 *
 * 对非负 v，C++ `%` 与 NumPy 一致；对负数补 +64 以与 Python 语义对齐（本探针 golden 全为非负）。
 */
__aicore__ inline void Func1Mod64(AscendC::LocalTensor<int8_t> &dst, AscendC::LocalTensor<int32_t> &src,
                                  uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i) {
        int32_t v = src.GetValue(i);
        int32_t m = v % 64;
        if (m < 0) {
            m += 64;
        }
        dst.SetValue(i, static_cast<int8_t>(m));
    }
}

} // namespace toy

#endif
