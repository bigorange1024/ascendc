#ifndef HAT_LINE18_PAIR_HPP
#define HAT_LINE18_PAIR_HPP

#include "basic.hpp"
#include "hat_vec.hpp"
#include "kernel_operator.h"
#include "mod_variants.hpp"
#include "tiling.h"

using AscendC::DataCopy;

namespace sepair {

/** se_pair 布局：ŝ_j 行号（全局 dst/src [8,256]）。 */
__aicore__ inline uint32_t s_row(uint16_t j)
{
    return (static_cast<uint32_t>(j) / 2U) * 4U + (static_cast<uint32_t>(j) % 2U) * 2U;
}

/** se_pair 布局：ê_p 行号。 */
__aicore__ inline uint32_t e_row(uint16_t p)
{
    return (static_cast<uint32_t>(p) / 2U) * 4U + (static_cast<uint32_t>(p) % 2U) * 2U + 1U;
}

/** 本 AIV 负责的输出行 p 下界（k=4：core0→0，core1→2）。 */
__aicore__ inline uint16_t p_begin(int32_t subCoreIdx)
{
    return static_cast<uint16_t>(subCoreIdx) * 2U;
}

__aicore__ inline uint16_t p_end(int32_t subCoreIdx)
{
    return static_cast<uint16_t>(subCoreIdx) * 2U + 2U;
}

/** 本地 / 远端列 j（k=4）。 */
__aicore__ inline bool j_local(int32_t subCoreIdx, uint16_t j)
{
    return (subCoreIdx == 0) ? (j < 2U) : (j >= 2U);
}

} // namespace sepair

/**
 * Alg.13 行 18（se_pair + 行切分）：
 *   AIV0 → t_hat[0:2]；AIV1 → t_hat[2:4]；
 *   partial-sum：本地列 basemul 后，从 GM 读对端 2 行 ŝ 补全内积；+ê 本地。
 */
class AivHatLine18Pair {
public:
    __aicore__ inline AivHatLine18Pair(int32_t subCoreIdx, uint32_t coeffN)
        : subCoreIdx(subCoreIdx), coeffN(coeffN), halfLen(coeffN / 2), pairCount(halfLen / 2),
          aHatTileLength(static_cast<uint32_t>(tiling::kHatK) * coeffN),
          aHatAivTileLength(static_cast<uint32_t>(sepair::p_end(subCoreIdx) - sepair::p_begin(subCoreIdx)) *
                            static_cast<uint32_t>(tiling::kHatK) * coeffN),
          seTileLength(static_cast<uint32_t>(tiling::kPolys) * coeffN),
          pBegin(sepair::p_begin(subCoreIdx)), pEnd(sepair::p_end(subCoreIdx))
    {
    }

    __aicore__ inline void Init(GM_ADDR t_hat, GM_ADDR shat_ehat, GM_ADDR a_hat)
    {
        gm_t.SetGlobalBuffer((__gm__ int32_t *)t_hat);
        gm_se.SetGlobalBuffer((__gm__ int32_t *)shat_ehat);
        gm_a.SetGlobalBuffer((__gm__ int32_t *)a_hat);
        pipe.InitBuffer(que_acc, 1, halfLen * sizeof(int32_t));
        pipe.InitBuffer(que_row, 1, halfLen * sizeof(int32_t));
        pipe.InitBuffer(que_line, 1, coeffN * sizeof(int32_t));
        pipe.InitBuffer(que_a_hat, 1, aHatAivTileLength * sizeof(int32_t));
        pipe.InitBuffer(que_se, 1, seTileLength * sizeof(int32_t));
        const uint32_t pc = pairCount;
        const uint32_t hl = halfLen;
        pipe.InitBuffer(scratch, (10U * pc + 4U * hl) * sizeof(int32_t));
#if F203_MOD_VARIANT == 2
        pipe.InitBuffer(calc_f, 3U * hl * sizeof(float));
#endif
    }

    __aicore__ inline void Process()
    {
        const uint32_t pc = pairCount;
        const uint32_t pcBytes = pc * sizeof(int32_t);
        const uint32_t hlBytes = halfLen * sizeof(int32_t);
        LocalTensor<int32_t> acc = que_acc.AllocTensor<int32_t>();
        LocalTensor<int32_t> row = que_row.AllocTensor<int32_t>();
        LocalTensor<int32_t> line = que_line.AllocTensor<int32_t>();
        LocalTensor<int32_t> se = que_se.AllocTensor<int32_t>();
        DataCopy(se, gm_se, seTileLength);
        KYBER_PIPE_ALL();
        LocalTensor<int32_t> a_tile = que_a_hat.AllocTensor<int32_t>();
        const uint32_t aAivBase = static_cast<uint32_t>(pBegin) * static_cast<uint32_t>(tiling::kHatK) * coeffN;
        DataCopy(a_tile, gm_a[aAivBase], aHatAivTileLength);
        KYBER_PIPE_ALL();
        LocalTensor<int32_t> f = scratch.GetWithOffset<int32_t>(halfLen, 10U * pcBytes);
        LocalTensor<int32_t> g = scratch.GetWithOffset<int32_t>(halfLen, 10U * pcBytes + hlBytes);
        LocalTensor<int32_t> t1m = scratch.GetWithOffset<int32_t>(halfLen, 10U * pcBytes + 2U * hlBytes);
        LocalTensor<int32_t> t2m = scratch.GetWithOffset<int32_t>(halfLen, 10U * pcBytes + 3U * hlBytes);
#if F203_MOD_VARIANT == 2
        const uint32_t fStride = halfLen * static_cast<uint32_t>(sizeof(float));
        LocalTensor<float> fRaw = calc_f.GetWithOffset<float>(halfLen, 0U);
        LocalTensor<float> fTmp = calc_f.GetWithOffset<float>(halfLen, fStride);
        LocalTensor<float> fQuot = calc_f.GetWithOffset<float>(halfLen, 2U * fStride);
#endif

        for (uint16_t p = pBegin; p < pEnd; ++p) {
            const uint32_t aPolyOff =
                (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin)) * aHatTileLength;
            const uint32_t eRowBase = sepair::e_row(p) * coeffN;
            DataCopy(line, se[eRowBase], coeffN);
            KYBER_PIPE_ALL();

            for (uint32_t subOff = 0; subOff < coeffN; subOff += halfLen) {
                const int32_t gammaOff = static_cast<int32_t>(subOff / halfLen) *
                                         static_cast<int32_t>(halfLen / 2);

                AscendC::Duplicate(acc, static_cast<int32_t>(0), static_cast<int32_t>(halfLen));
                KYBER_PIPE_ALL();

                for (uint16_t j = 0; j < tiling::kHatK; ++j) {
                    if (!sepair::j_local(subCoreIdx, j)) {
                        continue;
                    }
                    const uint32_t aOff = aPolyOff + static_cast<uint32_t>(j) * coeffN + subOff;
                    const uint32_t sOff = sepair::s_row(j) * coeffN + subOff;
                    DataCopy(f, a_tile[aOff], halfLen);
                    DataCopy(g, se[sOff], halfLen);
                    KYBER_PIPE_ALL();
                    multiply_ntts_half_scalar(row, f, g, static_cast<int32_t>(pairCount), gammaOff);
                    AscendC::Add(acc, acc, row, static_cast<int32_t>(halfLen));
                    KYBER_PIPE_ALL();
                }

                for (uint16_t j = 0; j < tiling::kHatK; ++j) {
                    if (sepair::j_local(subCoreIdx, j)) {
                        continue;
                    }
                    const uint32_t aOff = aPolyOff + static_cast<uint32_t>(j) * coeffN + subOff;
                    const uint32_t sOff = sepair::s_row(j) * coeffN + subOff;
                    DataCopy(f, a_tile[aOff], halfLen);
                    DataCopy(g, se[sOff], halfLen);
                    KYBER_PIPE_ALL();
                    multiply_ntts_half_scalar(row, f, g, static_cast<int32_t>(pairCount), gammaOff);
                    AscendC::Add(acc, acc, row, static_cast<int32_t>(halfLen));
                    KYBER_PIPE_ALL();
                }

                DataCopy(row, line[subOff], halfLen);
                KYBER_PIPE_ALL();
                AscendC::Add(acc, acc, row, static_cast<int32_t>(halfLen));
                KYBER_PIPE_ALL();
#if F203_MOD_VARIANT == 2
                MOD_Q_CAST(acc, kHatQ, t1m, fRaw, fTmp, fQuot, static_cast<int32_t>(halfLen));
#else
                MOD_Q_I32(acc, kHatQ, t1m, t2m, static_cast<int32_t>(halfLen));
#endif
                KYBER_PIPE_ALL();
                DataCopy(line[subOff], acc, halfLen);
                KYBER_PIPE_ALL();
            }

            const uint32_t dstBase = static_cast<uint32_t>(p) * coeffN;
            DataCopy(gm_t[dstBase], line, coeffN);
            KYBER_PIPE_ALL();
        }

        que_acc.FreeTensor(acc);
        que_row.FreeTensor(row);
        que_line.FreeTensor(line);
        que_a_hat.FreeTensor(a_tile);
        que_se.FreeTensor(se);
        KYBER_PIPE_ALL();
    }

private:
    const int32_t subCoreIdx;
    const uint32_t coeffN;
    const uint32_t halfLen;
    const uint32_t pairCount;
    const uint32_t aHatTileLength;
    const uint32_t aHatAivTileLength;
    const uint32_t seTileLength;
    const uint16_t pBegin;
    const uint16_t pEnd;
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_acc, que_row, que_line, que_a_hat, que_se;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratch;
#if F203_MOD_VARIANT == 2
    AscendC::TBuf<AscendC::TPosition::VECCALC> calc_f;
#endif
    AscendC::GlobalTensor<int32_t> gm_t, gm_se, gm_a;
};

#endif
