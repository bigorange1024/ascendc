/**
 * @file f203_encrypt_re_cbd.hpp
 * @brief Alg.14：PRF batch5 → 混合 CBD（η1=3×2 + η2=2×3）→ re GM（r‖e₁‖e₂）。
 *
 * 流水线位置（Encrypt prep 行 8–15 后半）：
 *   prf_out[5,192] uint8 → re[5,256] int32
 *   · row 0..1（r）：SamplePolyCBD_η1=3，读满 192B
 *   · row 2..4（e₁‖e₂）：SamplePolyCBD_η2=2，只读行内前 128B
 *     （与 squeeze(128) 等价：XOF 前缀性质）
 *
 * 复用 prep/alg8：CBD3 / CBD2 的 SWAR+LUT 单行例程；batch=5、block0 串行。
 * 与 golden：scripts/golden_encrypt_prep.build_re_from_coins / host_golden/golden_c.build_re。
 *
 * 背景：2026-07-27 相对 liboqs Encaps `c` 缺项根因——曾 5 行全走 η=2；按参数卡补缺。
 */
#pragma once

#include "f203_cbd_eta2.hpp"
#include "f203_cbd_eta3.hpp"
#include "f203_encrypt_prep_layout.h"

#include "kernel_operator.h"

namespace F203EncryptReCbd {

constexpr uint32_t N = F203EncryptPrep::kKyberN;
constexpr uint32_t PRF_STRIDE = F203EncryptPrep::kPrfBytesPerPoly;  // 192
constexpr uint32_t PRF_ETA2 = F203EncryptPrep::kPrfBytesEta2;       // 128
constexpr uint32_t RE_ROWS = F203EncryptPrep::kRePolys;
constexpr uint32_t R_ROWS = F203EncryptPrep::kReRRows;

/** GM→UB 拷入后同步 MTE2，再跑向量 CBD。 */
#define F203_ENCRYPT_CBD_SYNC_AFTER_COPYIN() AscendC::PipeBarrier<PIPE_MTE2>()
/** CBD 写完 rowLocal 后同步 PIPE_V，再 EnQue / DataCopy 出 GM。 */
#define F203_ENCRYPT_CBD_SYNC_BEFORE_COPYOUT() AscendC::PipeBarrier<PIPE_V>()

/**
 * 5 行混合 CBD：row 0..1→r（η1=3），2..3→e₁、4→e₂（η2=2）；re GM 行主序 int32[5,256]。
 *
 * @param prf_gm     PRF 输出 [5,192] uint8（行 stride=192）
 * @param re_gm      输出 [5,256] int32 扁平
 * @param scratchBuf 复用为单行 prfLocal[192]（uint8 视图）
 * @param rowQue     单行 int32[256] 输出队列（与 prep 的 prfYQue 共用）
 */
__aicore__ inline void SamplePolyCbdMixedBatchReWithUb(__gm__ const uint8_t *prf_gm, __gm__ int32_t *re_gm,
                                                       AscendC::TBuf<AscendC::TPosition::VECCALC> &scratchBuf,
                                                       AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
    AscendC::GlobalTensor<uint8_t> prfGm;
    AscendC::GlobalTensor<int32_t> reOutGm;
    prfGm.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prf_gm), RE_ROWS * PRF_STRIDE);
    reOutGm.SetGlobalBuffer(re_gm, RE_ROWS * N);

    // scratch 前 192B 作当前行 PRF 缓冲（η1 全用；η2 只用前 128B）
    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();

    for (uint32_t row = 0U; row < RE_ROWS; ++row) {
        // 统一拷 192B：η2 行多余 64B 不参与 CBD2 读
        AscendC::DataCopy(prfLocal, prfGm[row * PRF_STRIDE], PRF_STRIDE);
        F203_ENCRYPT_CBD_SYNC_AFTER_COPYIN();

        AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
        if (row < R_ROWS) {
            // r：η1=3
            F203CbdEta3::SamplePolyCbd3RowSwLutUb(rowLocal, prfLocal);
        } else {
            // e₁/e₂：η2=2（只解释前 PRF_ETA2=128 字节）
            (void)PRF_ETA2;
            F203CbdEta2::SamplePolyCbd2RowSwLutUb(rowLocal, prfLocal);
        }
        F203_ENCRYPT_CBD_SYNC_BEFORE_COPYOUT();

        rowQue.EnQue(rowLocal);
        rowLocal = rowQue.DeQue<int32_t>();
        AscendC::DataCopy(reOutGm[row * N], rowLocal, N);
        rowQue.FreeTensor(rowLocal);
    }
}

/** 兼容旧名：曾误称 BatchCbd2；现转调混合 CBD。 */
__aicore__ inline void SamplePolyCbd2BatchReWithUb(__gm__ const uint8_t *prf_gm, __gm__ int32_t *re_gm,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &scratchBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
    SamplePolyCbdMixedBatchReWithUb(prf_gm, re_gm, scratchBuf, rowQue);
}

}  // namespace F203EncryptReCbd
