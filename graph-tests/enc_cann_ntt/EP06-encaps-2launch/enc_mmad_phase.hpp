/**
 * EN15 · cann-ntt 矩阵化 NTT/INTT 单阶段（供 L2 compute 串级复用）
 *
 * 作用：把原 mmad_custom 循环体抽成可多次调用的阶段函数；核内仅用 flag 1/2/3
 * （AIV_SPLIT / AIC_MMAD / AIV_MERGE），禁止 GATE 4/8。
 * 调用约定：AIC 与双 AIV 必须同时进入本函数；阶段之间由调用方 SyncAll（Wait 环外）。
 * 背景：EN13 对 NTT/INTT 各独立 Host launch；EN15 要求 L2 单 launch 内串级。
 * 未采用：自研 SoftSync；把 SampleNTT/Matvec 融进本握手环。
 */
#ifndef ENC_MMAD_PHASE_HPP
#define ENC_MMAD_PHASE_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#include "aic_func.hpp"
#include "aiv_func.hpp"

namespace enc_mmad_phase {

enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT,
    AIC_MMAD,
    AIV_MERGE
};

__aicore__ inline void WaitFlag(MachineState state)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(state);
}

__aicore__ inline void SetFlag(MachineState state)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(state);
}

/**
 * 一次正向或逆向变换：src[bench,n] → dst[bench,n]；ws 前部已由 Host 填好 M4 digit。
 * @param dst 输出 GM
 * @param src 输入 GM
 * @param ws  workspace（M0..M3 + S* + A*）
 * @param tiling bench/q/Barrett；方向由 Host 选 M4_ntt 或 M4_intt 决定
 */
__aicore__ inline void RunCannNttPhase(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    const int32_t bench = tiling.bench;
    const int32_t q = tiling.q;
    const int32_t b_k = tiling.b_k;
    const int32_t b_mu = tiling.b_mu;
    const int32_t r28 = tiling.r28;
    const size_t n = tiling_one::n;
    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = (int32_t)AscendC::GetSubBlockIdx();
    const int32_t maxBenchTile = q == 3329 ? tiling_one::max_kernel_bench_tile_kyber
                                           : tiling_one::max_kernel_bench_tile;

    MachineState STATE;

    for (int32_t benchBase = 0; benchBase < bench; benchBase += maxBenchTile) {
        const int32_t tileBench =
            (bench - benchBase) < maxBenchTile ? (bench - benchBase) : maxBenchTile;

        const size_t M0 = 0;
        const size_t M1 = M0 + n * n;
        const size_t M2 = M1 + n * n;
        const size_t M3 = M2 + n * n;
        const size_t S0 = M3 + n * n;
        const size_t S1 = S0 + n * tileBench * sizeof(int8_t);
        const size_t S2 = S1 + n * tileBench * sizeof(int8_t);
        const size_t S3 = S2 + n * tileBench * sizeof(int8_t);
        const size_t A0 = S3 + n * tileBench * sizeof(int8_t);
        const size_t A1 = A0 + n * 4 * tileBench * sizeof(int32_t);
        const size_t A2 = A1 + n * 4 * tileBench * sizeof(int32_t);
        const size_t A3 = A2 + n * 4 * tileBench * sizeof(int32_t);

        if (AIC) {
            STATE = AIV_SPLIT;
            AicMmad cube((q == 3329 ? 2 : 4) * tileBench, n, n);
            cube.Init();
            WaitFlag(STATE);

            STATE = AIC_MMAD;
            if (q == 3329) {
                cube.Process<1, 0, 0, 0>(ws + A0, ws + S0, ws + M0);
                cube.Process<0, 1, 0, 0>(ws + A1, ws + S0, ws + M1);
            } else {
                cube.Process<1, 0, 0, 0>(ws + A0, ws + S0, ws + M0);
                cube.Process<1, 1, 0, 0>(ws + A1, ws + S0, ws + M1);
                cube.Process<1, 1, 0, 0>(ws + A2, ws + S0, ws + M2);
                cube.Process<0, 1, 0, 0>(ws + A3, ws + S0, ws + M3);
            }
            SetFlag(STATE);
        } else {
            const size_t dataOffset = (size_t)benchBase * n * sizeof(int32_t);

            STATE = AIV_SPLIT;
            {
                if (q == 3329) {
                    AivSplit2 split(subBlockID, n, tileBench);
                    split.Init(ws + S0, ws + S1, src + dataOffset);
                    split.CopyIn();
                    split.Compute();
                    split.CopyOut();
                } else {
                    AivSplit split(subBlockID, n, tileBench);
                    split.Init(ws + S0, ws + S1, ws + S2, ws + S3, src + dataOffset);
                    split.CopyIn();
                    split.Compute();
                    split.CopyOut();
                }
            }
            SetFlag(STATE);

            STATE = AIC_MMAD;
            WaitFlag(STATE);

            STATE = AIV_MERGE;
            {
                if (q == 3329) {
                    AivMergeKyber merge(subBlockID, n, tileBench);
                    merge.Init(dst + dataOffset, ws + A0, ws + A1);
                    merge.Process();
                } else {
                    AivMerge merge(subBlockID, n, tileBench, q, b_k, b_mu, r28);
                    merge.Init(dst + dataOffset, ws + A0, ws + A1, ws + A2, ws + A3);
                    merge.CopyIn();
                    merge.Compute();
                    merge.CopyOut();
                }
            }
        }
    }
}

} // namespace enc_mmad_phase

#endif // ENC_MMAD_PHASE_HPP
