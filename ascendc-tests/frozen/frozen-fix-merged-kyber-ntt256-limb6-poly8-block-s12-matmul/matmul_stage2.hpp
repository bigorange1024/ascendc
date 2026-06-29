#ifndef MATMUL_STAGE2_HPP
#define MATMUL_STAGE2_HPP

#include "kernel_operator.h"
#include "lib/matmul_intf.h"

using namespace matmul;

__aicore__ inline uint32_t CeilingU32(uint32_t a, uint32_t b)
{
    return (a + b - 1) / b;
}

__aicore__ inline void CopyCubeTiling(TCubeTiling *tiling, uint64_t &localMemSize, GM_ADDR tilingGM)
{
    uint32_t *ptr = reinterpret_cast<uint32_t *>(tiling);
    auto *tiling32 = reinterpret_cast<__gm__ uint32_t *>(tilingGM);
    for (uint32_t i = 0; i < sizeof(TCubeTiling) / sizeof(uint32_t); i++, ptr++) {
        *ptr = *(tiling32 + i);
    }
    localMemSize = *reinterpret_cast<__gm__ uint64_t *>(tilingGM + sizeof(TCubeTiling));
}

__aicore__ inline void CalcMatmulGmOffset(int blockIdx, const TCubeTiling &tiling, int &offsetA, int &offsetB,
                                          int &offsetC, int &tailM, int &tailN, bool isTransA, bool isTransB)
{
    const uint32_t mSingleBlocks = CeilingU32(tiling.M, tiling.singleCoreM);
    const uint32_t mCoreIndx = blockIdx % mSingleBlocks;
    const uint32_t nCoreIndx = blockIdx / mSingleBlocks;

    offsetA = mCoreIndx * tiling.Ka * tiling.singleCoreM;
    if (isTransA) {
        offsetA = mCoreIndx * tiling.singleCoreM;
    }
    offsetB = nCoreIndx * tiling.singleCoreN;
    if (isTransB) {
        offsetB = nCoreIndx * tiling.Kb * tiling.singleCoreN;
    }
    offsetC = mCoreIndx * tiling.N * tiling.singleCoreM + nCoreIndx * tiling.singleCoreN;

    tailM = tiling.M - mCoreIndx * tiling.singleCoreM;
    tailM = tailM < tiling.singleCoreM ? tailM : tiling.singleCoreM;

    tailN = tiling.N - nCoreIndx * tiling.singleCoreN;
    tailN = tailN < tiling.singleCoreN ? tailN : tiling.singleCoreN;
}

/** 手写 MIX Stage2：GM×GM Matmul，非 LeakyRelu 融合模板。C[16,512]=A[16,256]×B[256,512] */
__aicore__ inline void Stage2MatmulCube(GM_ADDR matAGm, GM_ADDR lutGm, GM_ADDR matCGm, GM_ADDR workspaceGm,
                                        GM_ADDR tilingGm, AscendC::TPipe *pipe)
{
    (void)workspaceGm;
    using A_T = int8_t;
    using B_T = int8_t;
    using C_T = int32_t;

    TCubeTiling tiling;
    uint64_t localMemSize = 0;
    CopyCubeTiling(&tiling, localMemSize, tilingGm);

    AscendC::GlobalTensor<A_T> aGlobal;
    AscendC::GlobalTensor<B_T> bGlobal;
    AscendC::GlobalTensor<C_T> cGlobal;
    aGlobal.SetGlobalBuffer(reinterpret_cast<__gm__ A_T *>(matAGm), tiling.M * tiling.Ka);
    bGlobal.SetGlobalBuffer(reinterpret_cast<__gm__ B_T *>(lutGm), tiling.Kb * tiling.N);
    cGlobal.SetGlobalBuffer(reinterpret_cast<__gm__ C_T *>(matCGm), tiling.M * tiling.N);

    int offsetA = 0;
    int offsetB = 0;
    int offsetC = 0;
    const bool isTransA = false;
    const bool isTransB = false;
    int tailM = 0;
    int tailN = 0;
    CalcMatmulGmOffset(AscendC::GetBlockIdx(), tiling, offsetA, offsetB, offsetC, tailM, tailN, isTransA, isTransB);

    auto gmA = aGlobal[offsetA];
    auto gmB = bGlobal[offsetB];
    auto gmC = cGlobal[offsetC];

    Matmul<MatmulType<AscendC::TPosition::GM, CubeFormat::ND, A_T>,
           MatmulType<AscendC::TPosition::GM, CubeFormat::ND, B_T>,
           MatmulType<AscendC::TPosition::GM, CubeFormat::ND, C_T>>
        mm;
    REGIST_MATMUL_OBJ(pipe, GetSysWorkSpacePtr(), mm, &tiling);
    (void)localMemSize;
    mm.SetTensorA(gmA, isTransA);
    mm.SetTensorB(gmB, isTransB);
    mm.SetTail(tailM, tailN);
    mm.IterateAll(gmC);
    mm.End();
}

#endif
