// @probe exp-mlkem-f203-pke-keygen-k4
// @file prep/alg7/f203_alg7_rej_vec.hpp
// @layer prep
// @role prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；含 XOF、rej 标量/向量与 compact LUT ROM。 / Alg.7 SampleNTT rejection-sampling prep kernels. 本文件 `f203_alg7_rej_vec.hpp` 为该子模块组件。 / Component: f203_alg7_rej_vec.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_alg7_interleave_rom.h, f203_alg7_layout.h, f203_alg7_rej_compact.hpp, f203_alg7_rej_filter.hpp, f203_alg7_rej_scalar.hpp, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_alg7_rej_vec.hpp
 * @brief Alg.7 向量 rejection sampling：剔除(d≥q) → Gather 交错 stream → 标量 compact 取 256。
 *
 * 流水线位置：d12_vec 在 ComputeD12Vec 之后调用 RejVecBulkFromD12Ub；
 * 依赖 rej_filter.hpp（剔除）、interleave_rom.h（Gather 索引）、rej_scalar.hpp（compact）。
 *
 * 工程不变量：
 *   - 剔除段禁止 for+GetValue（须 Mins 或 Compares+Select）
 *   - 交错段单次 Gather：d1[224]||d2[224] scratch → stream[448]
 *   - compact 段生产用标量（R5 向量 Compare+LUT 路线 SIM 未通过）
 *
 * 与 golden 关系：产出 â[256] 须与 f203_alg7_rej_scalar.c 语义一致。
 * 生产默认：F203_ALG7_REJ_IMPL=1（Mins 剔除）。
 */
#pragma once

#include "f203_alg7_interleave_rom.h"
#include "f203_alg7_layout.h"
#include "f203_alg7_rej_compact.hpp"
#include "f203_alg7_rej_filter.hpp"
#include "f203_alg7_rej_scalar.hpp"

#include "kernel_operator.h"

namespace F203Alg7 {

#define F203_ALG7_REJ_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

/**
 * rej 向量工作区布局（绑定 scratch 后半段，尺寸见 layout.h kRejVecWsInt32）。
 */
struct Alg7RejVecWs {
    AscendC::LocalTensor<int32_t> scratchT1;   // d1 平面拷贝 [224]
    AscendC::LocalTensor<int32_t> scratchT2;   // d2 平面拷贝 [224]
    AscendC::LocalTensor<int32_t> stream;      // 交错候选 [448]
    AscendC::LocalTensor<int32_t> idxRom;      // Gather 字节索引 ROM 驻留 UB [448]
    AscendC::LocalTensor<uint8_t> cmpMaskUb;   // Compares 掩码 128B（IMPL=2）
    AscendC::LocalTensor<int32_t> filterTileUb; // 128 int32 tile 缓冲（IMPL=2）
};

/**
 * 将 rej scratch int32 基址切分为各工作张量。
 * 偏移公式见 layout.h：core = 2×224 + 448 + 448 索引 + 掩码 + filter tile。
 */
__aicore__ inline void BindAlg7RejVecWs(AscendC::LocalTensor<int32_t> &base, Alg7RejVecWs &w)
{
    w.scratchT1 = base[0];
    w.scratchT2 = base[kCandPairs];
    w.stream = base[kCandPairs * 2U];
    w.idxRom = base[kCandPairs * 2U + kStreamLen];
    w.cmpMaskUb = base[kRejVecWsCoreInt32].ReinterpretCast<uint8_t>();
    w.filterTileUb = base[kRejVecWsCoreInt32 + kRejFilterMaskUbInt32];
}

/**
 * Init：将 interleave ROM 常量拷入 idxRom UB（Init 阶段允许短标量循环；热路径无 GetValue）。
 */
__aicore__ inline void InitAlg7InterleaveRomUb(AscendC::LocalTensor<int32_t> &idxRom)
{
    for (uint32_t i = 0U; i < kInterleaveRomLen; ++i) {
        idxRom.SetValue(i, kAlg7InterleaveReorderByte[i]);
    }
    F203_ALG7_REJ_PIPE_ALL();
}

/**
 * d1||d2 平面 → stream[448] 交错：先 DataCopy 到 scratch，再单次 Gather。
 *
 * 交错语义：stream[2k]=d1[k], stream[2k+1]=d2[k]（与 Alg.7 顺序 rej 一致）。
 * Gather 索引为 scratch 拼接缓冲内的 4 字节对齐偏移（见 interleave_rom.h）。
 */
__aicore__ inline void InterleaveD12GatherUb(Alg7RejVecWs &w, const AscendC::LocalTensor<int32_t> &d1,
                                             const AscendC::LocalTensor<int32_t> &d2)
{
    using AscendC::DataCopy;
    using AscendC::Gather;

    DataCopy(w.scratchT1, d1, kCandPairs);
    DataCopy(w.scratchT2, d2, kCandPairs);
    F203_ALG7_REJ_PIPE_ALL();
    // 从 scratchT1 起始的连续 int32 视图做 Gather（索引指向 d1[k] 与 d2[k] 字节位置）
    Gather(w.stream, w.scratchT1, w.idxRom.ReinterpretCast<uint32_t>(), 0U, kInterleaveStreamLen);
    F203_ALG7_REJ_PIPE_ALL();
}

/**
 * 向量 rej 主入口：in-place 剔除 → 交错 → 标量 compact。
 *
 * @param d1In   输入 d1[224]（通常来自 d1Que，不被原地破坏）
 * @param d2In   输入 d2[224]
 * @param d1Work 工作 UB，剔除后 d1'[224]（拒绝 lane =q）
 * @param d2Work 工作 UB，剔除后 d2'[224]
 * @param tmp    预留（当前未用）
 * @param ws     rej 向量工作区（已 Bind + InitRom）
 * @param aOut   输出 â[256]
 * @return       写入系数个数
 */
__aicore__ inline uint32_t RejVecBulkFromD12Ub(const AscendC::LocalTensor<int32_t> &d1In,
                                               const AscendC::LocalTensor<int32_t> &d2In,
                                               AscendC::LocalTensor<int32_t> &d1Work,
                                               AscendC::LocalTensor<int32_t> &d2Work,
                                               AscendC::LocalTensor<int32_t> &tmp,
                                               Alg7RejVecWs &ws, AscendC::LocalTensor<int32_t> &aOut)
{
    using AscendC::DataCopy;

    DataCopy(d1Work, d1In, kCandPairs);
    DataCopy(d2Work, d2In, kCandPairs);
    F203_ALG7_REJ_PIPE_ALL();

    // 向量剔除：d≥q 的 lane 标记为 q（拒绝），接受 lane 保持原值
    RejectFilterDispatchUb(d1Work, d1Work, ws.cmpMaskUb, ws.filterTileUb);
    RejectFilterDispatchUb(d2Work, d2Work, ws.cmpMaskUb, ws.filterTileUb);
    InterleaveD12GatherUb(ws, d1Work, d2Work);
    (void)tmp;
    // R5 向量 compact（Compare+LUT）SIM 掩码读法未通过；生产用标量 compact 扫 stream
    return RejScalarCompactStreamUb(ws.stream, kStreamLen, aOut);
}

}  // namespace F203Alg7
