/**
 * @file f203_alg7_rej_scalar.hpp
 * @brief 设备端标量 rej：UB LocalTensor 上执行，语义同 f203_alg7_rej_scalar.c。
 *
 * 流水线位置：
 *   - F203_ALG7_REJ_IMPL=0：d12_vec 全链直接调用 RejScalarFromD12Ub
 *   - F203_ALG7_REJ_IMPL=1/2：向量剔除+交错后，compact 段调用 RejScalarCompactStreamUb
 *
 * 与 golden 关系：标量路径为向量 rej 的语义金标准；输出 â[256] 须与 gen_data.py 一致。
 */
#pragma once

#include "f203_alg7_layout.h"
#include "f203_alg7_rej_scalar.h"

#include "kernel_operator.h"

namespace F203Alg7 {

/**
 * 从 UB 平面 d1/d2 顺序 rej，写入 aOut（长度 kKyberN=256）。
 *
 * @param d1     UB int32[kCandPairs]，Alg.7 line 7 输出
 * @param d2     UB int32[kCandPairs]
 * @param aOut   UB int32[kKyberN] 输出 â
 * @param npairs 候选对数（224）
 * @return       写入系数个数；672B 固定预 squeeze 下应等于 kKyberN
 *
 * 前置条件：d1/d2/aOut 已绑定有效 UB；热路径使用 GetValue/SetValue（仅 REJ_IMPL=0）。
 */
__aicore__ inline uint32_t RejScalarFromD12Ub(const AscendC::LocalTensor<int32_t> &d1,
                                              const AscendC::LocalTensor<int32_t> &d2,
                                              AscendC::LocalTensor<int32_t> &aOut, uint32_t npairs)
{
    const int32_t q = static_cast<int32_t>(kKyberQ);
    uint32_t j = 0U;
    for (uint32_t i = 0U; i < npairs && j < kKyberN; ++i) {
        const int32_t v1 = d1.GetValue(i);
        if (v1 < q) {
            aOut.SetValue(j, v1);
            ++j;
            if (j >= kKyberN) {
                break;
            }
        }
        const int32_t v2 = d2.GetValue(i);
        if (v2 < q && j < kKyberN) {
            aOut.SetValue(j, v2);
            ++j;
        }
    }
    return j;
}

/**
 * 交错 stream 上的标量 compact：跳过拒绝标记（v==q），按序取前 kKyberN 个合法系数。
 *
 * @param stream    UB int32[kStreamLen=448]，d1/d2 交错；拒绝 lane 已标为 q
 * @param streamLen 扫描长度（448）
 * @param aOut      UB int32[kKyberN] 输出
 * @return          实际写入个数（应等于 256）
 *
 * 背景：向量 rej 生产路径在剔除+Gather 后调用本函数（R5 向量 compact 暂未启用）。
 */
__aicore__ inline uint32_t RejScalarCompactStreamUb(const AscendC::LocalTensor<int32_t> &stream, uint32_t streamLen,
                                                    AscendC::LocalTensor<int32_t> &aOut)
{
    const int32_t q = static_cast<int32_t>(kKyberQ);
    uint32_t j = 0U;
    for (uint32_t k = 0U; k < streamLen && j < kKyberN; ++k) {
        const int32_t v = stream.GetValue(k);
        if (v < q) {  // 接受：v∈[0,q-1]；拒绝：Mins/Select 已将 lane 置 q
            aOut.SetValue(j, v);
            ++j;
        }
    }
    return j;
}

}  // namespace F203Alg7
