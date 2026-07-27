/**
 * @file f203_encrypt_re_cbd.hpp
 * @brief Alg.14：PRF batch5 → CBD_η=2 batch5 → re GM（r‖e₁‖e₂ 连续 5 行）。
 *
 * 流水线位置（Encrypt prep 行 8–15 后半）：
 *   prf_out[5,128] → SamplePolyCBD_η=2 ×5 → re[5,256] int32
 *   行语义：row 0..1→r，2..3→e₁，4→e₂
 *
 * 复用 k2 prep/alg8 SWAR+LUT 单行 CBD；batch=5、block0 串行（F203_CBD_BLOCK_DIM=1）。
 * 与 golden：scripts/golden_encrypt_prep.sample_poly_cbd2 / build_re_from_coins。
 */
#pragma once

#include "f203_cbd_eta2.hpp"
#include "f203_encrypt_prep_layout.h"

#include "kernel_operator.h"

namespace F203EncryptReCbd {

constexpr uint32_t N = F203EncryptPrep::kKyberN;
constexpr uint32_t PRF_BYTES = F203EncryptPrep::kPrfBytesPerPoly;
constexpr uint32_t RE_ROWS = F203EncryptPrep::kRePolys;

/** GM→UB 拷入后同步 MTE2，再跑向量 CBD。 */
#define F203_ENCRYPT_CBD_SYNC_AFTER_COPYIN() AscendC::PipeBarrier<PIPE_MTE2>()
/** CBD 写完 rowLocal 后同步 PIPE_V，再 EnQue / DataCopy 出 GM。 */
#define F203_ENCRYPT_CBD_SYNC_BEFORE_COPYOUT() AscendC::PipeBarrier<PIPE_V>()

/**
 * 5 行 CBD：row 0..1→r，2..3→e₁，4→e₂；re GM 行主序 int32[5,256]。
 *
 * @param prf_gm     PRF 输出 [5,128] uint8
 * @param re_gm      输出 [5,256] int32 扁平
 * @param scratchBuf 复用为单行 prfLocal[128]（uint8 视图）
 * @param rowQue     单行 int32[256] 输出队列（与 prep 的 prfYQue 共用）
 */
__aicore__ inline void SamplePolyCbd2BatchReWithUb(__gm__ const uint8_t *prf_gm, __gm__ int32_t *re_gm,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &scratchBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
    AscendC::GlobalTensor<uint8_t> prfGm;
    AscendC::GlobalTensor<int32_t> reOutGm;
    prfGm.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prf_gm), RE_ROWS * PRF_BYTES);
    reOutGm.SetGlobalBuffer(re_gm, RE_ROWS * N);

    // scratch 前 128B 作当前行 PRF 缓冲
    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();

    for (uint32_t row = 0U; row < RE_ROWS; ++row) {
        // 逐行 GM→UB：128B PRF
        AscendC::DataCopy(prfLocal, prfGm[row * PRF_BYTES], PRF_BYTES);
        F203_ENCRYPT_CBD_SYNC_AFTER_COPYIN();

        AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
        // SWAR + CBD2_AB_LUT → 256 系数
        F203CbdEta2::SamplePolyCbd2RowSwLutUb(rowLocal, prfLocal);
        F203_ENCRYPT_CBD_SYNC_BEFORE_COPYOUT();

        rowQue.EnQue(rowLocal);
        rowLocal = rowQue.DeQue<int32_t>();
        AscendC::DataCopy(reOutGm[row * N], rowLocal, N);
        rowQue.FreeTensor(rowLocal);
    }
}

}  // namespace F203EncryptReCbd
