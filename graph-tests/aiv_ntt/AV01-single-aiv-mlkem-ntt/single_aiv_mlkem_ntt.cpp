/**
 * @file single_aiv_mlkem_ntt.cpp
 * @brief AV01：单 AIV、单 poly 的 ML-KEM（n=256,q=3329）正向 NTT。
 *
 * 抽取自 thirdparty/ntt（910B 单 AIV NTT），去掉 ML-DSA 分支与 tiling 参数。
 * KERNEL_TYPE_AIV_ONLY；chained Gather 层间置换 + lazy Montgomery 蝶形；不用 Cube。
 *
 * 与 Tag5T「NTT S1–S3 禁 Gather」正交（本路径 Gather 仅层间重排）。
 * 与 EN01 cann-ntt（MIX/Cube）对照：同为 ML-KEM 正向 NTT，I/O=256×int32。
 */
#include "kernel_operator.h"
#include "constants.hpp"

using namespace AscendC;
using namespace single_ntt;

/**
 * 单 AIV ML-KEM NTT：7 层蝶形 + 末次 Gather → 标准 bit-reversed 二次因子对序，
 * 再 lazy 收尾到 [0,q)。
 */
class SingleAivMlKemNtt {
public:
    __aicore__ inline void Run(GM_ADDR input, GM_ADDR output, GM_ADDR roots, GM_ADDR indices)
    {
        constexpr int rootWords = kKemRootWords;
        constexpr int mapWords = 7 * 256;
        pipe.InitBuffer(dataBuf, 1024);
        pipe.InitBuffer(packBuf, 1024);
        pipe.InitBuffer(tBuf, 1024);
        pipe.InitBuffer(termBuf, 512);
        pipe.InitBuffer(sBuf, 1024);
        pipe.InitBuffer(rootBuf, rootWords * 4);
        pipe.InitBuffer(mapBuf, 16384);
        auto data = dataBuf.Get<int32_t>();
        auto spare = packBuf.Get<int32_t>();
        auto rt = rootBuf.Get<int32_t>();
        auto map = mapBuf.Get<uint32_t>();
        GlobalTensor<int32_t> x, y, zeta;
        GlobalTensor<uint32_t> offsets;
        x.SetGlobalBuffer((__gm__ int32_t *)input, 256);
        y.SetGlobalBuffer((__gm__ int32_t *)output, 256);
        zeta.SetGlobalBuffer((__gm__ int32_t *)roots, rootWords);
        offsets.SetGlobalBuffer((__gm__ uint32_t *)indices, mapWords);
        DataCopy(data, x, 256);
        DataCopy(rt, zeta, rootWords);
        DataCopy(map, offsets, mapWords);
        PipeBarrier<PIPE_ALL>();
        for (int stage = 0; stage < 7; ++stage) {
            if (stage == 0) {
                Butterfly(data, data[128], rt, stage);
            } else {
                Gather(spare, data, map[(stage - 1) * 256], 0U, 256);
                Butterfly(spare, spare[128], rt, stage);
                auto previous = data;
                data = spare;
                spare = previous;
            }
        }
        // 末次：蝶形拼接布局 → 标准 bit-reversed（非自然频率序）
        Gather(spare, data, map[6 * 256], 0U, 256);
        data = spare;
        {
            auto t = tBuf.Get<int32_t>();
            auto s = sBuf.Get<int32_t>();
            Muls(s, data, 20159, 256);
            Adds(s, s, 1 << 25, 256);
            ShiftRight(s, s, 26, 256);
            Muls(s, s, 3329, 256);
            Sub(data, data, s, 256);
            ShiftRight(t, data, 31, 256);
            Muls(t, t, -3329, 256);
            Add(data, data, t, 256);
        }
        PipeBarrier<PIPE_ALL>();
        DataCopy(y, data, 256);
        PipeBarrier<PIPE_ALL>();
    }

private:
    /**
     * 一层 128 宽蝶形：t=mont(b·ζ)；b←a-t；a←a+t。
     * ζ = rt[stage*128 ..]。
     */
    __aicore__ inline void Butterfly(LocalTensor<int32_t> a, LocalTensor<int32_t> b,
                                     LocalTensor<int32_t> rt, int stage)
    {
        auto t = tBuf.Get<int32_t>();
        auto s = sBuf.Get<int32_t>();
        Mul(t, b, rt[stage * 128], 128);
        Muls(s, t, -3327, 128);
        ShiftLeft(s, s, 16, 128);
        ShiftRight(s, s, 16, 128);
        Muls(s, s, 3329, 128);
        Sub(t, t, s, 128);
        ShiftRight(t, t, 16, 128);
        Sub(b, a, t, 128);
        Add(a, a, t, 128);
    }

    TPipe pipe;
    TBuf<TPosition::VECCALC> dataBuf, packBuf, tBuf, termBuf, sBuf, rootBuf, mapBuf;
};

/**
 * @param input   GM int32[256]，系数 ∈ [0,q)
 * @param output  GM int32[256]
 * @param roots   GM int32[1024] = kKemRoots
 * @param indices GM uint32[1792] = kKemPermutations（字节偏移）
 */
extern "C" __global__ __aicore__ void single_aiv_mlkem_ntt(GM_ADDR input, GM_ADDR output,
                                                           GM_ADDR roots, GM_ADDR indices)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    SingleAivMlKemNtt kernel;
    kernel.Run(input, output, roots, indices);
}
