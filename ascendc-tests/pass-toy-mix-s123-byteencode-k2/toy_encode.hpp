#ifndef TOY_MIX_S123_ENCODE_HPP
#define TOY_MIX_S123_ENCODE_HPP

/**
 * @file toy_encode.hpp
 * @brief Stage3 末尾的「编码」玩具：对应真流程里的 ByteEncode₁₂，此处简化为 x % 64 → int8。
 *
 * 流水线位置：被 aiv_func.hpp 的 Stage3 段调用，是本探针 MIX 三阶段
 * （S1 AIV 填数 → S2 AIC MMAD → S3+encode AIV 后处理）的最后一步，
 * 在 UB 内把 Cube 输出的 int32 结果做 Adds(+1) 后转换为 int8 写出（对应真实
 * 流程中 Stage3 向量后处理 + ByteEncode₁₂ 打包，此处简化为取模运算，
 * 不做真正的 12-bit 位打包）。
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
    /* 逐元素标量循环：len 通常为 kOutPerAiv=2048（每 AIV 负责一半 out），
     * 规模小，本探针刻意用标量循环模拟真流程 ByteEncode 的「UB 内嵌 C 标量循环」约束
     * （见 TOY_MIX_S123.md「UB 驻留约定」），不追求向量化性能。 */
    for (uint32_t i = 0; i < len; ++i) {
        int32_t v = src.GetValue(i);
        int32_t m = v % 64;
        /* C++ 的 % 对负数结果可能为负（与数学 mod 不同），补 +64 校正到 [0,64) 区间，
         * 使其与 Python `%` 运算（Python 取模结果符号跟随除数）语义一致 */
        if (m < 0) {
            m += 64;
        }
        dst.SetValue(i, static_cast<int8_t>(m));
    }
}

} // namespace toy

#endif
