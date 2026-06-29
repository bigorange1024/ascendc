#ifndef BYTE_ENCODE12_PAIR_HPP
#define BYTE_ENCODE12_PAIR_HPP

#include "hat_line18_pair.hpp"
#include "kernel_operator.h"
#include "tiling.h"

using AscendC::DataCopy;

namespace byte_encode12 {

constexpr uint32_t kPolyBytes = 384U;
constexpr uint32_t kPolysPerAiv = 2U;
constexpr uint32_t kAivShardBytes = kPolysPerAiv * kPolyBytes;

/** FIPS 203 Alg.5 ByteEncode₁₂ — UB 内嵌 C 标量。 */
__aicore__ inline void poly_byte_encode12_local(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a, uint32_t coeffN)
{
    const uint32_t pairs = coeffN / 2U;
    for (uint32_t i = 0; i < pairs; ++i) {
        const uint16_t t0 = static_cast<uint16_t>(a.GetValue(2U * i) & 0xFFF);
        const uint16_t t1 = static_cast<uint16_t>(a.GetValue(2U * i + 1U) & 0xFFF);
        r.SetValue(3U * i + 0U, static_cast<uint8_t>(t0 & 0xFFU));
        r.SetValue(3U * i + 1U, static_cast<uint8_t>((t0 >> 8) | ((t1 << 4) & 0xF0U)));
        r.SetValue(3U * i + 2U, static_cast<uint8_t>(t1 >> 4));
    }
}

} // namespace byte_encode12

/**
 * Alg.13 行 19–20（se_pair + 行切分）：
 *   每 AIV 对本地 2 条 t̂[p]、ŝ[s_row(p)] 在 UB 内 ByteEncode，再写出 ek/sk 片。
 */
class AivByteEncode1920Pair {
public:
    __aicore__ inline AivByteEncode1920Pair(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx(subCoreIdx), coeffN(coeffN),
          pBegin(sepair::p_begin(subCoreIdx)), pEnd(sepair::p_end(subCoreIdx))
    {
    }

    __aicore__ inline void Init(GM_ADDR ek_polyvec, GM_ADDR sk_polyvec, GM_ADDR t_hat, GM_ADDR shat_ehat)
    {
        gm_ek.SetGlobalBuffer((__gm__ uint8_t *)ek_polyvec);
        gm_sk.SetGlobalBuffer((__gm__ uint8_t *)sk_polyvec);
        gm_t.SetGlobalBuffer((__gm__ int32_t *)t_hat);
        gm_s.SetGlobalBuffer((__gm__ int32_t *)shat_ehat);
        pipe.InitBuffer(que_poly, 1, coeffN * sizeof(int32_t));
        pipe.InitBuffer(que_ek, 1, byte_encode12::kAivShardBytes * sizeof(uint8_t));
        pipe.InitBuffer(que_sk, 1, byte_encode12::kAivShardBytes * sizeof(uint8_t));
    }

    __aicore__ inline void Process()
    {
        LocalTensor<int32_t> poly = que_poly.AllocTensor<int32_t>();
        LocalTensor<uint8_t> ek_local = que_ek.AllocTensor<uint8_t>();
        LocalTensor<uint8_t> sk_local = que_sk.AllocTensor<uint8_t>();

        for (uint16_t p = pBegin; p < pEnd; ++p) {
            const uint32_t localIdx = static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin);
            const uint32_t byteLocal = localIdx * byte_encode12::kPolyBytes;
            const uint32_t byteGlobal = static_cast<uint32_t>(p) * byte_encode12::kPolyBytes;

            DataCopy(poly, gm_t[static_cast<uint32_t>(p) * coeffN], coeffN);
            KYBER_PIPE_ALL();
            LocalTensor<uint8_t> ek_poly = ek_local[byteLocal];
            byte_encode12::poly_byte_encode12_local(ek_poly, poly, coeffN);
            KYBER_PIPE_ALL();
            DataCopy(gm_ek[byteGlobal], ek_poly, byte_encode12::kPolyBytes);
            KYBER_PIPE_ALL();

            const uint32_t sRow = sepair::s_row(p);
            DataCopy(poly, gm_s[sRow * coeffN], coeffN);
            KYBER_PIPE_ALL();
            LocalTensor<uint8_t> sk_poly = sk_local[byteLocal];
            byte_encode12::poly_byte_encode12_local(sk_poly, poly, coeffN);
            KYBER_PIPE_ALL();
            DataCopy(gm_sk[byteGlobal], sk_poly, byte_encode12::kPolyBytes);
            KYBER_PIPE_ALL();
        }

        que_poly.FreeTensor(poly);
        que_ek.FreeTensor(ek_local);
        que_sk.FreeTensor(sk_local);
        KYBER_PIPE_ALL();
    }

private:
    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint16_t pBegin;
    const uint16_t pEnd;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_poly;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_ek, que_sk;
    AscendC::GlobalTensor<uint8_t> gm_ek, gm_sk;
    AscendC::GlobalTensor<int32_t> gm_t, gm_s;
};

#endif
