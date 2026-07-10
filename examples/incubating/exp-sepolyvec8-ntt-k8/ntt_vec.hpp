/**
 * @file ntt_vec.hpp
 * @brief sepolyvec8 NTT 向量原语：limb6 分裂 + Barrett/wrap mod。
 *
 * 流水线位置：AivSplit.Compute 调 split_vec；AivMerge.Compute 调 barrett_mul_vec_runtime。
 * 对齐：ML-KEM limb6（kKyberLimbBits=6）；本探针为 kPolys=8 batch NTT。
 * 与 golden：仅 I/O 等价；禁止把 Host/参考源码当 AscendC 规格。
 */
#ifndef __NTT_VEC_HPP__
#define __NTT_VEC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

/** split_vec 输出：x0=lo limb 打包、x1=hi limb 打包（每 4 个 int8 压成 int32 写回）。 */
struct Tensor_int8x4 {
    LocalTensor<int8_t> &x0;
    LocalTensor<int8_t> &x1;
};

/** LocalTensor 类型重解释（不改底层存储）。 */
template <typename U, typename T>
__aicore__ static inline auto tr(LocalTensor<T> x)
{
    return x.template ReinterpretCast<U>();
}

/**
 * 单系数 → 两个 6-bit limb。
 * @param d0 lo = a & 63；@param d1 hi = (a>>6) & 63
 */
__aicore__ static inline void split_2xint6(int8_t &d0, int8_t &d1, int32_t a)
{
    d0 = static_cast<int8_t>(a & kKyberLimbMask);
    d1 = static_cast<int8_t>((a >> kKyberLimbBits) & kKyberLimbMask);
}

/**
 * 向量 limb6 分裂：每 4 个 int32 → 两组 int8×4 打包写 dst.x0/x1。
 * @param count 元素个数（须为 4 的倍数）
 */
__aicore__ inline void split_vec(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, const int32_t count)
{
    auto x0 = tr<int32_t>(dst.x0);
    auto x1 = tr<int32_t>(dst.x1);

    // 按 4 宽打包：标量 GetValue/SetValue（本探针路径）
    for (int32_t i = 0; i < count / 4; i++) {
        int32_t a[4] = {src.GetValue(i * 4 + 0), src.GetValue(i * 4 + 1), src.GetValue(i * 4 + 2),
                        src.GetValue(i * 4 + 3)};

        int8_t d0[4], d1[4];
        for (int j = 0; j < 4; j++) {
            split_2xint6(d0[j], d1[j], a[j]);
        }
        x0.SetValue(i, *(int32_t *)d0);
        x1.SetValue(i, *(int32_t *)d1);
    }
}

/**
 * 将 src 约化到 [0,q)：若 x>=q 则 x-q，否则保留（用符号位选路）。
 * @param t1/t2 临时 UB；@param count 向量长度
 */
__aicore__ inline void wrap_mod_vec_runtime(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &src, int32_t q,
                                            LocalTensor<int32_t> &t1, LocalTensor<int32_t> &t2, int32_t count)
{
    AscendC::Adds(t1, src, -q, count);
    auto &t1_u32 = *reinterpret_cast<LocalTensor<uint32_t> *>(&t1);
    auto &t2_u32 = *reinterpret_cast<LocalTensor<uint32_t> *>(&t2);
    AscendC::ShiftRight(t2_u32, t1_u32, 31U, count);
    AscendC::Mul(t2, src, t2, count);
    AscendC::Max(dst, t1, t2, count);
}

/**
 * Barrett 约化一步（就地改 dst）再 wrap_mod。
 * 本探针 Merge 末尾连调两次（与 ntt_sim 约定一致）。
 */
__aicore__ inline void barrett_mul_vec_runtime(LocalTensor<int32_t> &dst, int32_t q, int32_t k, int32_t mu,
                                               LocalTensor<int32_t> &t1, LocalTensor<int32_t> &t2, int32_t count)
{
    AscendC::ShiftRight(t1, dst, static_cast<int32_t>(k - 1), count);
    AscendC::Muls(t1, t1, mu, count);
    AscendC::ShiftRight(t1, t1, static_cast<int32_t>(k + 1), count);
    AscendC::Muls(t1, t1, q, count);
    AscendC::Sub(dst, dst, t1, count);
    wrap_mod_vec_runtime(dst, dst, q, t1, t2, count);
}

#endif
