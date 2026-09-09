/**
 * @file kg_prep_custom.cpp
 * @brief KGR-P01 / RB-K01：PKE KeyGen L1 prep — seed_d → (ρ,σ) → Â + ŝ/ê。
 *
 * 本文件在流水线中的位置：KeyGen 重建三+一 launch 拓扑的 **L1 AIV-only**（KB §B2）。
 * 不做 NTT / 点积 / ByteEncode；写出中间张量供后续 L2 NTT 刀消费。
 *
 * 积木接线（**禁**抄 KeyGen 算子树 / frozen；**禁**大段照抄探针核进本目录）：
 *   - Â：活跃 `pass-fix-f203-alg13-lines3-7-a-hat-k4` → `F203Ahat16::BuildAHat16ShardFromSeedD`
 *   - ŝ/ê：活跃 `pass-fix-f203-alg13-lines8-15-se-k4` → `F203SeVector::BuildSrcFromSeedD`
 *     （内含 G→σ、SHAKE256 PRF、Alg.8 CBD η=2；V3 生产默认）
 *
 * Seed 约定：`SEED_D` uint32 → Derand SHA3-256("exp-mlkem-f203-2s1e-k4:SEED_D=<dec>")
 * → G(d‖byte(k=4)) SHA3-512 → ρ‖σ；默认 SEED_D=20260619。
 *
 * 背景：首版 blockDim=1，规避 prep 双 AIV 半写 Â 失败史（ProcessInline 契约留给并行优化刀）。
 * 结论：KERNEL_TYPE_AIV_ONLY、零 CrossCore/MIX；业务写出由积木内 UB+DataCopy 完成。
 * 未采用：fork KeyGen mmad_custom / examples keygen；blockDim=2。
 */
// 须在 include 积木头之前锁定单 AIV（ascendc_library 偶发吃不到外层 definitions）
#ifndef F203_AHAT16_BLOCK_DIM
#define F203_AHAT16_BLOCK_DIM 1
#endif
#ifndef F203_AHAT16_BATCH_SHAKE
#define F203_AHAT16_BATCH_SHAKE 0
#endif
#ifndef F203_ALG7_REJ_IMPL
#define F203_ALG7_REJ_IMPL 1
#endif
#ifndef F203_ALG7_D12_GATHER
#define F203_ALG7_D12_GATHER 0
#endif
#ifndef F203_ALG7_XOF_504
#define F203_ALG7_XOF_504 0
#endif
#ifndef F203_CBD_BLOCK_DIM
#define F203_CBD_BLOCK_DIM 1
#endif

#include "f203_a_hat16_ub.hpp"
#include "f203_se_vector.hpp"
#include "kernel_operator.h"

#include <cstdint>

namespace {
constexpr uint32_t kK = 4U;
constexpr uint32_t kPolyN = 256U;
constexpr uint32_t kAHatPolys = kK * kK;          // 16
constexpr uint32_t kSrcRows = 2U * kK;            // 8 = ŝ×4 + ê×4
constexpr uint32_t kSHatCoeffs = kK * kPolyN;     // 1024
constexpr uint32_t kECoeffs = kK * kPolyN;        // 1024
}  // namespace

/**
 * AIV-only KeyGen prep。
 * @param seedDGm   输入 GM：uint32 SEED_D（小端 4B）
 * @param aHatGm    输出 GM：int32[16,256] 行主序 Â，offset=(p*4+j)*256
 * @param sHatGm    输出 GM：int32[4,256] ŝ（CBD 前 4 行）
 * @param eGm       输出 GM：int32[4,256] ê（CBD 后 4 行）
 * @param prfWsGm   工作区 GM：uint8[8,128] PRF 中间态（对拍可选 dump）
 * @param srcWsGm   工作区 GM：int32[8,256] CBD 扁平等价布局（行 0..3=ŝ，4..7=ê）
 * @param xWsGm     工作区 GM：PRF 消息占位（UB 路径未读；与 lines8-15 入口形参对齐）
 * @param lenWsGm   工作区 GM：PRF lengths 占位（同上）
 * @param tilingGm  工作区 GM：ShakeGeneralTilingData（Host 填 SHAKE256 PRF tiling）
 * 前置条件：blockDim=1；KERNEL_TYPE_AIV_ONLY；无 CrossCore。
 */
extern "C" __global__ __aicore__ void kg_prep_custom(GM_ADDR seedDGm, GM_ADDR aHatGm, GM_ADDR sHatGm, GM_ADDR eGm,
                                                     GM_ADDR prfWsGm, GM_ADDR srcWsGm, GM_ADDR xWsGm, GM_ADDR lenWsGm,
                                                     GM_ADDR tilingGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    // ---------- 1) 读 SEED_D（标量；4B）----------
    const __gm__ uint32_t *seedPtr = reinterpret_cast<const __gm__ uint32_t *>(seedDGm);
    const uint32_t seed_d = seedPtr[0];
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---------- 2) Â ← SampleNTT(ρ)：积木一次 Derand+G+16×Alg.7 ----------
    // 背景：BuildAHat16ShardFromSeedD 自建 TPipe；F203_AHAT16_BLOCK_DIM=1 跑满 16 poly。
    F203Ahat16::BuildAHat16ShardFromSeedD(seed_d, reinterpret_cast<__gm__ int32_t *>(aHatGm), 0U);
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---------- 3) src[8,256] ← PRF(σ)+CBD_η2：积木 G→P→C（V3）----------
    F203SeVector::BuildSrcFromSeedD(seed_d, xWsGm, lenWsGm, prfWsGm, srcWsGm, tilingGm);
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---------- 4) 扁平 src → 业务输出 ŝ / ê（UB+DataCopy；禁 GlobalTensor::SetValue）----------
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    pipe.InitBuffer(rowQue, 1, kPolyN * sizeof(int32_t));

    AscendC::GlobalTensor<int32_t> gmSrc;
    AscendC::GlobalTensor<int32_t> gmSHat;
    AscendC::GlobalTensor<int32_t> gmE;
    gmSrc.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(srcWsGm), kSrcRows * kPolyN);
    gmSHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(sHatGm), kSHatCoeffs);
    gmE.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(eGm), kECoeffs);

    // ŝ：src 行 0..3
    for (uint32_t r = 0U; r < kK; ++r) {
        AscendC::LocalTensor<int32_t> row = rowQue.AllocTensor<int32_t>();
        AscendC::DataCopy(row, gmSrc[r * kPolyN], kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        rowQue.EnQue(row);
        row = rowQue.DeQue<int32_t>();
        AscendC::DataCopy(gmSHat[r * kPolyN], row, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        rowQue.FreeTensor(row);
    }

    // ê：src 行 4..7
    for (uint32_t r = 0U; r < kK; ++r) {
        AscendC::LocalTensor<int32_t> row = rowQue.AllocTensor<int32_t>();
        AscendC::DataCopy(row, gmSrc[(kK + r) * kPolyN], kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        rowQue.EnQue(row);
        row = rowQue.DeQue<int32_t>();
        AscendC::DataCopy(gmE[r * kPolyN], row, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        rowQue.FreeTensor(row);
    }

    (void)kAHatPolys;
}
