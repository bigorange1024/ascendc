/**
 * @file mmad_custom.cpp
 * @brief Kyber 风格单 poly（n=256）MIX NTT 核：AIV Split → AIC Mmad×2 → AIV Merge+Barrett。
 *
 * 流水线位置：本用例设备入口（Host 见 main.cpp）。
 * 来源：原 thirdparty/merged_kyber，作者授权迁入 ascendc-tests（见 ORIGIN.md）。
 * 与 golden：I/O 对齐 scripts/ntt_sim_kyber.py；非 FIPS Tag5T / MlkemNtt 交付路径。
 */
/**
 * @file mmad_custom.cpp
 * @brief Kyber 单 poly n=256 MIX NTT 核：AIV Split → AIC Mmad×2 → AIV Merge+Barrett。
 *
 * 流水线位置：本用例唯一设备核；host 见 main.cpp；golden 见 scripts/ntt_sim_kyber.py。
 * 背景：由原 thirdparty/merged_kyber（MmadBiasInvocation1）授权迁入 ascendc-tests；
 *       非 FIPS Tag5T / MlkemNtt 交付路径（见 STATUS.md）。
 */
#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#include "aic_func.hpp"
#include "aiv_func.hpp"

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

extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    /* Algorithm "int32 Mmad"
        AIV             AIC
        split ------>  16x mmad
                        |
        merge <-----------
         ↓
        barrett_reduce 
    */
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;
    using tiling::M0, tiling::M1, tiling::M2, tiling::M3;

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = (int32_t)AscendC::GetSubBlockIdx();
    const auto n = tiling.tileLength;

    MachineState STATE;
    
    if (AIC) {
        // AIC Core
        STATE = AIV_SPLIT;
        AicMmad mmad(2, n, n);
        mmad.Init();
        WAIT

        STATE = AIC_MMAD;
        mmad.Process(ws + A0, ws + S0, ws + M0);
        mmad.Process(ws + A1, ws + S0, ws + M1);
        // mmad.Process(ws + A2, ws + S0, ws + M2);
        // mmad.Process(ws + A3, ws + S0, ws + M3);
        SET
    } 
    else {
        // AIV Core 0 & 1
        STATE = AIV_SPLIT;
        { 
            // 将 SPLIT 任务平均分配到两个 AIV 核

            AivSplit split(subBlockID, n / 2);
            size_t src_offset = subBlockID * (n / 2) * sizeof(int32_t);
            size_t dst_offset = subBlockID * (n / 2) * sizeof(int8_t);

            split.Init(ws + S0 + dst_offset, ws + S1 + dst_offset, 
                       ws + S2 + dst_offset, ws + S3 + dst_offset, 
                       src + src_offset);
            split.CopyIn();
            split.Compute();
            split.CopyOut();
            if(subBlockID == 1) {
                // AivGmProbe(ws + S0 + dst_offset, int8_t, n / 2);
                // AivGmProbe(ws + S1 + dst_offset, int8_t, n / 2);
                // AivGmProbe(ws + S2 + dst_offset, int8_t, n / 2);
                // AivGmProbe(ws + S3 + dst_offset, int8_t, n / 2);
            } 
            SET
        }

        STATE = AIC_MMAD;
        WAIT

        if(subBlockID == 0) {
            // AivGmProbe(ws + A0, int32_t, n * 4);
            // AivGmProbe(ws + A1, int32_t, n * 4);
            // AivGmProbe(ws + A2, int32_t, n * 4);
            // AivGmProbe(ws + A3, int32_t, n * 4);
        }
        STATE = AIV_MERGE;
        {
            AivMerge merge(subBlockID, n, 3329);
            merge.Init(dst + subBlockID * n / 2 * sizeof(int32_t), 
                ws + A0, ws + A1, ws + A2, ws + A3);
            merge.CopyIn();
            merge.Compute();
            merge.CopyOut();
            // if(subBlockID == 0)
                // AivGmProbe(dst + subBlockID * n / 2 * sizeof(int32_t), int32_t, n / 2);
        }



    }
}

/* Algorithm "Kyber NTT"
    如 scripts/ntt_sim_kyber.py
    将蝶形运算转换为列变换矩阵，算出 M
    Cooley-Turkey 等价于 F = f @ M
*/


/* Algorithm "Bailey 4-Step NTT"
    input: n1, n2 <= n, n1 * n2 = n
    input: a[x]
    input: w[k]
    input: w0[k]
    input: w1[k]
    output: NTT(a) bit-reversed order

STEP 1. 重排输入 → reshape 成矩阵
for i in range(n1):
    for j in range(n2):
        B[i,j] = a[i*n2 + j]
B = B.T

STEP 2. 对每行做 FFT/NTT
for j in range(n2):
    B[j] = NTT(B_j, w0, n1, q)
B = B.T

STEP 3. 乘 twiddle factor（矩阵乘法）
for i in range(n1):
    for j in range(n2):
        B[i,j] *= w[i*2 + j] 
        B[i,j] %= q

STEP 4. 对每列做 FFT/NTT
for j in range(n2):
    B[i] = NTT(B_i, w1, n2, q)

B = B.T

for j in range(n2):
    for i in range(n1):
        a[j*n1 + i] = B[j,i]

return a

*/