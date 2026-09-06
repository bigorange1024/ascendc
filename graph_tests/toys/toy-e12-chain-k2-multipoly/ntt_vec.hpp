#ifndef __NTT_VEC_HPP__
#define __NTT_VEC_HPP__
#include "kernel_operator.h"
#include "basic.hpp"

struct Tensor_int8x4 {
    LocalTensor<int8_t> &x0;
    LocalTensor<int8_t> &x1;
    // LocalTensor<int8_t> &x2;
    // LocalTensor<int8_t> &x3;
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

__aicore__ static inline void split_2xint7(int8_t &d0, int8_t &d1, int32_t a) {
    d0 = a & 0x7f;
    d1 = (a >> 7) & 0x7f;
    // d2 = (a >> 14) & 0x7f;
    // d3 = (a >> 21) & 0x7f;
    // __assertion_info("%x %x %x %x <- %x", d0, d1, d2, d3, a);
}

__aicore__ inline void split_vec(
    Tensor_int8x4 &dst,
    LocalTensor<int32_t> &src,
    // LocalTensor<int32_t> &t1,
    const int32_t count
) {
    auto x0 = tr<int32_t>(dst.x0);
    auto x1 = tr<int32_t>(dst.x1);
    // auto x2 = tr<int32_t>(dst.x2);
    // auto x3 = tr<int32_t>(dst.x3);

    for(int32_t i = 0; i < count / 4; i++) {
        int32_t a[4] =  {src.GetValue(i * 4 + 0),
                         src.GetValue(i * 4 + 1),
                         src.GetValue(i * 4 + 2),
                         src.GetValue(i * 4 + 3)};

        int8_t d0[4], d1[4];
        for(int j = 0; j < 4; j++) {
            split_2xint7(d0[j], d1[j], a[j]);
        }
        x0.SetValue(i, *(int32_t*)d0);
        x1.SetValue(i, *(int32_t*)d1);
        // x2.SetValue(i, *(int32_t*)d2);
        // x3.SetValue(i, *(int32_t*)d3);
    }
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


#endif