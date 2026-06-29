#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "launch_profile.h"
#include "tiling/platform/platform_ascendc.h"
#include "tiling/tiling_api.h"

using namespace matmul_tiling;

namespace {
constexpr int32_t kM = 16;
constexpr int32_t kN = 512;
constexpr int32_t kK = 256;
constexpr int32_t kBaseM = 16;
constexpr int32_t kBaseN = 32;
constexpr int32_t kBaseK = -1;
} // namespace

void GenerateTiling(const char *socVersion, uint8_t *tilingBuf)
{
    const auto cfg = launch_profile::Get(launch_profile::FromEnv());

    optiling::TCubeTiling tilingData;
    auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance(socVersion);
    MultiCoreMatmulTiling tilingApi(*ascendcPlatform);

    tilingApi.SetDim(cfg.usedCoreNum);
    tilingApi.SetAType(TPosition::GM, CubeFormat::ND, DataType::DT_INT8, false);
    tilingApi.SetBType(TPosition::GM, CubeFormat::ND, DataType::DT_INT8, false);
    tilingApi.SetCType(TPosition::GM, CubeFormat::ND, DataType::DT_INT32);
    tilingApi.SetBias(false);
    tilingApi.SetOrgShape(kM, kN, kK);
    tilingApi.SetShape(kM, kN, kK);
    tilingApi.SetSingleShape(cfg.singleCoreM, cfg.singleCoreN, kK);
    tilingApi.SetTraverse(MatrixTraverse::FIRSTM);
    tilingApi.SetFixSplit(kBaseM, kBaseN, kBaseK);
    tilingApi.SetBufferSpace(-1, -1, -1);

    if (tilingApi.GetTiling(tilingData) == -1) {
        std::cerr << "gen tiling failed profile=" << launch_profile::Name(cfg.profile) << std::endl;
        std::exit(1);
    }
    tilingData.set_stepM(1);
    tilingData.set_stepN(1);

    const uint32_t tcubeTilingSize = tilingData.GetDataSize();
    tilingData.SaveToBuffer(tilingBuf, tcubeTilingSize);

    uint64_t localMemSize = 0;
    ascendcPlatform->GetCoreMemSize(platform_ascendc::CoreMemType::UB, localMemSize);
    *reinterpret_cast<uint64_t *>(tilingBuf + tcubeTilingSize) = localMemSize;

    std::cerr << "[tiling] profile=" << launch_profile::Name(cfg.profile) << " M=" << kM << " N=" << kN << " K=" << kK
              << " blockDim=" << cfg.blockDim << " usedCoreNum=" << cfg.usedCoreNum
              << " singleCoreM=" << tilingData.get_singleCoreM()
              << " singleCoreN=" << tilingData.get_singleCoreN() << std::endl;
}
