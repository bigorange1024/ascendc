/**
 * Host tiling: C[M,N]=A[M,K]xB[K,N]，形状由环境变量 MATMUL_* 控制（默认单核 128x512x512）。
 */
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "matmul_shape.h"
#include "tiling/platform/platform_ascendc.h"
#include "tiling/tiling_api.h"

using namespace matmul_tiling;

namespace {
constexpr int32_t kBaseM = 16;
constexpr int32_t kBaseN = 32;
constexpr int32_t kBaseK = -1;
} // namespace

void GenerateTiling(const char *socVersion, uint8_t *tilingBuf)
{
    const int32_t m = matmul_shape::M();
    const int32_t n = matmul_shape::N();
    const int32_t k = matmul_shape::K();
    const int32_t usedCoreNum = matmul_shape::UsedCoreNum();
    const int32_t singleM = matmul_shape::SingleCoreM();
    const int32_t singleN = matmul_shape::SingleCoreN();

    optiling::TCubeTiling tilingData;
    auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance(socVersion);
    MultiCoreMatmulTiling tilingApi(*ascendcPlatform);

    tilingApi.SetDim(usedCoreNum);
    tilingApi.SetAType(TPosition::GM, CubeFormat::ND, DataType::DT_INT8, false);
    tilingApi.SetBType(TPosition::GM, CubeFormat::ND, DataType::DT_INT8, false);
    tilingApi.SetCType(TPosition::GM, CubeFormat::ND, DataType::DT_INT32);
    tilingApi.SetBias(false);
    tilingApi.SetOrgShape(m, n, k);
    tilingApi.SetShape(m, n, k);
    tilingApi.SetSingleShape(singleM, singleN, k);
    tilingApi.SetTraverse(MatrixTraverse::FIRSTM);
    tilingApi.SetFixSplit(kBaseM, kBaseN, kBaseK);
    tilingApi.SetBufferSpace(-1, -1, -1);

    if (tilingApi.GetTiling(tilingData) == -1) {
        std::cerr << "gen tiling failed M=" << m << " N=" << n << " K=" << k << std::endl;
        std::exit(1);
    }
    tilingData.set_stepM(1);
    tilingData.set_stepN(1);

    const uint32_t tcubeTilingSize = tilingData.GetDataSize();
    tilingData.SaveToBuffer(tilingBuf, tcubeTilingSize);

    uint64_t localMemSize = 0;
    ascendcPlatform->GetCoreMemSize(platform_ascendc::CoreMemType::UB, localMemSize);
    *reinterpret_cast<uint64_t *>(tilingBuf + tcubeTilingSize) = localMemSize;

    std::cerr << "[tiling] M=" << m << " N=" << n << " K=" << k << " usedCoreNum=" << usedCoreNum
              << " blockDim(env)=" << matmul_shape::BlockDim() << " singleCoreM=" << tilingData.get_singleCoreM()
              << " singleCoreN=" << tilingData.get_singleCoreN() << std::endl;
}
