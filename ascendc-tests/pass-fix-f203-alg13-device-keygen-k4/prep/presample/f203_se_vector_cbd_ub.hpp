
/** CBD_η=2 的 UB 路径：读 prf_out，写 src poly。 */
// @probe pass-fix-f203-alg13-device-keygen-k4
// @file prep/presample/f203_se_vector_cbd_ub.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样与 NTT17 链入口；从 seed 派生设备侧中间量供 alg7/alg8/ahat。 / Presample + Keccak/PRF device vector entry. 本文件 `f203_se_vector_cbd_ub.hpp` 为该子模块组件。 / Component: f203_se_vector_cbd_ub.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_cbd_eta2.hpp, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 行 8–15 PRF+CBD presample 链。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/presample/f203_se_vector_cbd_ub.hpp
 */
/**
 * @file f203_se_vector_cbd_ub.hpp
 * @brief V2.5 实验 Phase C：一次 bulk GM→UB DataCopy；CBD 无逐行 prf GM 读（更慢，不接入集成）。
 */
#pragma once

#include "f203_cbd_eta2.hpp"

#include "kernel_operator.h"

namespace F203SeVector {

#define F203_SE_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

__aicore__ inline void FillPrfRowFromUb(AscendC::LocalTensor<uint8_t> &prfRowLocal,
                                        const AscendC::LocalTensor<uint8_t> &prfAllUb, uint32_t row)
{
    const uint32_t base = row * F203CbdEta2::PRF_BYTES;
    for (uint32_t b = 0; b < F203CbdEta2::PRF_BYTES; ++b) {
        prfRowLocal.SetValue(b, prfAllUb.GetValue(base + b));
    }
}

/** 单 TPipe：bulk prf GM→UB + 8 行 SWAR+LUT + src DataCopy。 */
__aicore__ inline void BuildSrcFromPrfGmUb(__gm__ const uint8_t *prf_out_gm, __gm__ int32_t *src_gm)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> prfAllQue;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> prfRowQue;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    pipe.InitBuffer(prfAllQue, 1, F203CbdEta2::PRF_TOTAL_BYTES);
    pipe.InitBuffer(prfRowQue, 1, F203CbdEta2::PRF_BYTES);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(F203CbdEta2::N) * sizeof(int32_t));

    AscendC::LocalTensor<uint8_t> prfAllUb = prfAllQue.AllocTensor<uint8_t>();
    AscendC::GlobalTensor<uint8_t> prfGm;
    prfGm.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prf_out_gm), F203CbdEta2::PRF_TOTAL_BYTES);
    AscendC::DataCopy(prfAllUb, prfGm, F203CbdEta2::PRF_TOTAL_BYTES);
    F203_SE_PIPE_ALL();

    AscendC::GlobalTensor<int32_t> srcGm;
    srcGm.SetGlobalBuffer(src_gm, F203CbdEta2::SRC_COEFFS);

    for (uint32_t row = 0; row < F203CbdEta2::ROWS; ++row) {
        AscendC::LocalTensor<uint8_t> prfRowLocal = prfRowQue.AllocTensor<uint8_t>();
        FillPrfRowFromUb(prfRowLocal, prfAllUb, row);
        F203_SE_PIPE_ALL();
        prfRowQue.EnQue(prfRowLocal);
        prfRowLocal = prfRowQue.DeQue<uint8_t>();

        AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
        F203CbdEta2::SamplePolyCbd2RowSwLutUb(rowLocal, prfRowLocal);
        prfRowQue.FreeTensor(prfRowLocal);
        F203_SE_PIPE_ALL();

        rowQue.EnQue(rowLocal);
        rowLocal = rowQue.DeQue<int32_t>();
        AscendC::DataCopy(srcGm[row * F203CbdEta2::N], rowLocal, F203CbdEta2::N);
        F203_SE_PIPE_ALL();
        rowQue.FreeTensor(rowLocal);
    }

    prfAllQue.FreeTensor(prfAllUb);
}

}  // namespace F203SeVector
