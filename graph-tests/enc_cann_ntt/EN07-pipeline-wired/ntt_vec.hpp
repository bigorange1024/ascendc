/**
 * EN01-kem256-ntt-port · 向量模约 / digit 辅助
 *
 * 迁入来源：thirdparty/cann-ntt-author-merged_dsa/ntt_vec.hpp
 * 注意：此处为 digit-split / Barrett 向量原语，不是 butterfly NTT 本体。
 */
#ifndef __NTT_VEC_HPP__
#define __NTT_VEC_HPP__
#include "kernel_operator.h"
#include "basic.hpp"
#include <cstdint>

struct Tensor_int8x4 {
    LocalTensor<int8_t> &x0;
    LocalTensor<int8_t> &x1;
    LocalTensor<int8_t> &x2;
    LocalTensor<int8_t> &x3;
};

struct Tensor_int8x2 {
    LocalTensor<int8_t> &x0;
    LocalTensor<int8_t> &x1;
};

template <typename U, typename T>
__aicore__ static inline auto tr(LocalTensor<T> x) {
    return x.template ReinterpretCast<U>();
} 

// __aicore__ static inline void split_4xint7(int8_t &d0, int8_t &d1, int8_t &d2, int8_t &d3, int32_t a) {
//     d0 = a & 0x7f;
//     d1 = (a >> 7) & 0x7f;
//     d2 = (a >> 14) & 0x7f;
//     d3 = (a >> 21) & 0x7f;
//     // __assertion_info("%x %x %x %x <- %x", d0, d1, d2, d3, a);
// }

__aicore__ static inline void split_2xint7_scalar(int8_t &d0, int8_t &d1, int32_t a) {
    d0 = a & 0x7f;
    d1 = (a >> 7) & 0x7f;
    // d2 = (a >> 14) & 0x7f;
    // d3 = (a >> 21) & 0x7f;
    // __assertion_info("%x %x %x %x <- %x", d0, d1, d2, d3, a);
}

__aicore__ inline void split_vec_int7x2(
    Tensor_int8x2        &dst,
    LocalTensor<int32_t> &src,
    LocalTensor<int32_t> &t1,
    LocalTensor<half>    &t2,
    const int32_t        count
) {
    using namespace AscendC;
    SetDeqScale((half)1.0f);

    ShiftRight(tr<uint32_t>(t1), tr<uint32_t>(src), (uint32_t)7, count);
    Cast(t2, t1, RoundMode::CAST_NONE, count);
    Cast(dst.x1, t2, RoundMode::CAST_NONE, count);

    ShiftLeft(t1, t1, 7, count);
    Sub(t1, src, t1, count);
    Cast(t2, t1, RoundMode::CAST_NONE, count);
    Cast(dst.x0, t2, RoundMode::CAST_NONE, count);
}

__aicore__ inline void split_vec_int7x4(
    Tensor_int8x4        &dst,
    LocalTensor<int32_t> &src,
    LocalTensor<int32_t> &t1,
    LocalTensor<half>    &t2,
    const int32_t        count
) {
    #ifdef ASCENDC_CPU_DEBUG
        std::vector<int32_t> tx(count), ty(count);
        for (int i = 0; i < count; i++) 
            tx[i] = src.GetValue(i);
    #endif
    using namespace AscendC;    
    SetDeqScale((half)1.0f);

    ShiftRight(tr<uint32_t>(t1), tr<uint32_t>(src), (uint32_t)(7 * 3), count);
    Cast(t2, t1, RoundMode::CAST_NONE, count);
    Cast(dst.x3, t2, RoundMode::CAST_NONE, count);
    
    ShiftLeft(t1, t1, 7 * 3, count);
    Sub(src, src, t1, count);
    ShiftRight(tr<uint32_t>(t1), tr<uint32_t>(src), (uint32_t)(7 * 2), count);
    Cast(t2, t1, RoundMode::CAST_NONE, count);
    Cast(dst.x2, t2, RoundMode::CAST_NONE, count);

    ShiftLeft(t1, t1, 7 * 2, count);
    Sub(src, src, t1, count);
    ShiftRight(tr<uint32_t>(t1), tr<uint32_t>(src), (uint32_t)(7 * 1), count);
    Cast(t2, t1, RoundMode::CAST_NONE, count);
    Cast(dst.x1, t2, RoundMode::CAST_NONE, count);

    ShiftLeft(t1, t1, 7 * 1, count);
    Sub(t1, src, t1, count);
    Cast(t2, t1, RoundMode::CAST_NONE, count);
    Cast(dst.x0, t2, RoundMode::CAST_NONE, count);

    #ifdef ASCENDC_CPU_DEBUG
        // V→S：向量 Cast 后 Scalar GetValue 前须 PIPE_V 屏障（sync_audit SYNC-02）
        AscendC::PipeBarrier<PIPE_V>();
        for (int i = 0; i < count; i++) {
            ty[i] = dst.x0.GetValue(i) ^ (dst.x1.GetValue(i) << 7) ^ (dst.x2.GetValue(i) << 14) ^ (dst.x3.GetValue(i) << 21);
        }
        __check_diff_vec(tx, ty, count);
    #endif
}


__aicore__ inline void wrap_mod_vec_runtime(
    LocalTensor<int32_t>& dst, 
    LocalTensor<int32_t>& src,
    int32_t q, 
    LocalTensor<int32_t>& t1, 
    LocalTensor<int32_t>& t2, 
    int32_t count)
{
    AscendC::  Adds(t1, src, -q, count);
    auto& t1_u32 = *reinterpret_cast<LocalTensor<uint32_t>*>(&t1);
    auto& t2_u32 = *reinterpret_cast<LocalTensor<uint32_t>*>(&t2);
    AscendC:: ShiftRight(t2_u32, t1_u32, 31U, count);
    AscendC::  Mul(t2, src, t2, count);
    AscendC:: Max(dst, t1, t2, count);
}


__aicore__ inline void barrett_mul_vec_runtime(
    LocalTensor<int32_t>& dst,
    int32_t q,
    int32_t k, 
    int32_t mu,
    LocalTensor<int32_t>& t1,
    LocalTensor<int32_t>& t2,
    int32_t count)
{
    // AscendC::Mul(dst, src, w, count);
    AscendC::ShiftRight(t1, dst, (int32_t)(k - 1), count);
    AscendC::Muls(t1, t1, mu, count);
    AscendC::ShiftRight(t1, t1, (int32_t)(k + 1), count);
    AscendC::Muls(t1, t1, q, count);
    AscendC::Sub(dst, dst, t1, count);
    wrap_mod_vec_runtime(dst, dst, q, t1, t2, count);
}


__aicore__ inline void kyber_wrap_mod_vec(
    LocalTensor<int32_t>& dst,
    LocalTensor<int32_t>& t1,
    LocalTensor<int32_t>& t2,
    const int32_t count)
{
    AscendC::Adds(t1, dst, -3329, count);
    auto& t1_u32 = *reinterpret_cast<LocalTensor<uint32_t>*>(&t1);
    auto& t2_u32 = *reinterpret_cast<LocalTensor<uint32_t>*>(&t2);
    AscendC::ShiftRight(t2_u32, t1_u32, 31U, count);
    AscendC::Mul(t2, dst, t2, count);
    AscendC::Max(dst, t1, t2, count);
}

__aicore__ inline void kyber_barrett_reduce_once_vec(
    LocalTensor<int32_t>& dst,
    LocalTensor<int32_t>& t1,
    LocalTensor<int32_t>& t2,
    const int32_t count)
{
    AscendC::ShiftRight(t1, dst, 11, count);
    AscendC::Muls(t1, t1, 5039, count);
    AscendC::ShiftRight(t1, t1, 13, count);
    AscendC::Muls(t1, t1, 3329, count);
    AscendC::Sub(dst, dst, t1, count);
    kyber_wrap_mod_vec(dst, t1, t2, count);
}

__aicore__ inline void kyber_barrett_reduce_twice_vec(
    LocalTensor<int32_t>& dst,
    LocalTensor<int32_t>& t1,
    LocalTensor<int32_t>& t2,
    const int32_t count)
{
    kyber_barrett_reduce_once_vec(dst, t1, t2, count);
    kyber_barrett_reduce_once_vec(dst, t1, t2, count);
}

#endif
