/**
 * EN03-encrypt-host-skel · 矩阵化 NTT/INTT MIX 核（与 EN02/EN01 同构）
 *
 * 基线：自 EN02 复制积木。Host 五段中仅 L2/L4 使用本核（独立 launch）：
 *   L2 喂正向 M4_ntt；L4 喂 inverse M4_intt——核内不区分方向。
 * Prep/Matvec/Pack 在独立 AIV 桩核，禁止融进本 MIX。
 * q=3329 走 2-digit Kyber 路径；禁 GATE 4/8。
 * CrossCore：仅保留作者包既有握手（AIV_SPLIT / AIC_MMAD），勿改（B1/B2）。
 */
#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#include "aic_func.hpp"
#include "aiv_func.hpp"

// CrossCore 握手状态：与作者包一致；flag 值即枚举数值，禁改语义（B1/sync_audit）
enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT,
    AIC_MMAD,
    AIV_MERGE
};

__aicore__ inline void __WAIT(MachineState STATE, const bool AIC, const int32_t subBlockID) {
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(STATE);
}

__aicore__ inline void __SET(MachineState STATE, const bool AIC, const int32_t subBlockID) {
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(STATE);
}

#define WAIT __WAIT(STATE, AIC, subBlockID);
#define SET  __SET(STATE, AIC, subBlockID);

/**
 * MIX AIC×1 + AIV×2：按 bench 分块做矩阵化正向 NTT。
 * @param dst  输出系数 [bench,n] int32
 * @param src  输入系数 [bench,n] int32
 * @param ws   workspace：前部 M0..M3 为变换矩阵 digit；随后 S* / A* 为 split/mmad 缓冲
 * @param tiling bench/q/Barrett 参数；本刀默认 q=3329
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    const int32_t bench = tiling.bench;
    const int32_t q = tiling.q;
    const int32_t b_k = tiling.b_k;
    const int32_t b_mu = tiling.b_mu;
    const int32_t r28 = tiling.r28;
    const size_t n = tiling_one::n;
    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = (int32_t)AscendC::GetSubBlockIdx();
    // Kyber q=3329：单核可吃更大 bench tile（64）；DSA 大模数用更小 tile
    const int32_t maxBenchTile = q == 3329
                               ? tiling_one::max_kernel_bench_tile_kyber
                               : tiling_one::max_kernel_bench_tile;

    MachineState STATE;

    for (int32_t benchBase = 0; benchBase < bench; benchBase += maxBenchTile) {
        const int32_t tileBench =
            (bench - benchBase) < maxBenchTile
                ? (bench - benchBase)
                : maxBenchTile;

        // workspace 偏移：M* = int8 矩阵 digit 平面；S* = 输入 digit；A* = MMAD int32 结果
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
            // AIC：等 AIV split 完成 → 对 digit 平面做 Cube MMAD → 通知 AIV merge
            STATE = AIV_SPLIT;
            AicMmad cube((q == 3329 ? 2 : 4) * tileBench, n, n);
            cube.Init();
            WAIT

            STATE = AIC_MMAD;
            if (q == 3329) {
                // Kyber：仅两路 7-bit digit（低/高）× 对应矩阵平面
                cube.Process<1, 0, 0, 0>(ws + A0, ws + S0, ws + M0);
                cube.Process<0, 1, 0, 0>(ws + A1, ws + S0, ws + M1);
            } else {
                cube.Process<1, 0, 0, 0>(ws + A0, ws + S0, ws + M0);
                cube.Process<1, 1, 0, 0>(ws + A1, ws + S0, ws + M1);
                cube.Process<1, 1, 0, 0>(ws + A2, ws + S0, ws + M2);
                cube.Process<0, 1, 0, 0>(ws + A3, ws + S0, ws + M3);
            }
            SET
        } else {
            const size_t dataOffset = (size_t)benchBase * n * sizeof(int32_t);

            // AIV：系数 → digit split，写 S*；再等 AIC；最后 merge 回 dst
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
            SET

            STATE = AIC_MMAD;
            WAIT

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

/* Algorithm "Kyber NTT"
    See scripts/ntt_sim_kyber.py. The butterfly computation is converted into
    a column-transform matrix M, so F = f @ M.
*/

/* Algorithm "Bailey 4-Step NTT"
    input: n1, n2 <= n, n1 * n2 = n
    input: a[x]
    input: w[k], w0[k], w1[k]
    output: NTT(a) in bit-reversed order
*/