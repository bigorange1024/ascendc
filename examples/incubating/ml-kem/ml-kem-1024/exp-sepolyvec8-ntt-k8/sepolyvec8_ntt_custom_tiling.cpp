/**
 * @file sepolyvec8_ntt_custom_tiling.cpp
 * @brief Host 侧生成 Stage2 Cube tiling（C[16,512]=A[16,256]×B[256,512] int8→int32）。
 *
 * 流水线位置：run 前由工具链/脚本调用 GenerateTiling，写入 tiling 缓冲供 Matmul API 使用。
 * 剖面：LAUNCH_PROFILE=aicore=1|4（见 launch_profile.h）。
 */
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

/**
 * 按 socVersion 与 LAUNCH_PROFILE 填充 TCubeTiling + UB 大小尾缀。
 * @param socVersion 如 Ascend910B4；@param tilingBuf 输出缓冲
 */
void GenerateTiling(const char *socVersion, uint8_t *tilingBuf)
{
    const auto cfg = launch_profile::Get(launch_profile::FromEnv());

    optiling::TCubeTiling tilingData;
    auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance(socVersion);
    MultiCoreMatmulTiling tilingApi(*ascendcPlatform);

    // 绑定 A/B/C dtype 与 Org/Single 形状
    tilingApi.SetDim(cfg.usedCoreNum);
    tilingApi.SetAType(TPosition::GM, CubeFormat::ND, DataType::DT_INT8, false);
    tilingApi.SetBType(TPosition::GM, CubeFormat::ND, DataType::DT_INT8, false);
    tilingApi.SetCType(TPosition::GM, CubeFormat::ND, DataType::DT_INT32);
    tilingApi.SetBias(false);
    tilingApi.SetOrgShape(kM, kN, kK);
    tilingApi.SetShape(kM, kN, kK);
    if (cfg.useSetSingleShape) {
        tilingApi.SetSingleShape(cfg.singleCoreM, cfg.singleCoreN, kK);
    }
    tilingApi.SetTraverse(MatrixTraverse::FIRSTM);
    tilingApi.SetFixSplit(kBaseM, kBaseN, kBaseK);
    tilingApi.SetBufferSpace(-1, -1, -1);

    // 取 tiling；失败则退出（剖面/形状不合法）
    int64_t res = tilingApi.GetTiling(tilingData);
    if (res == -1) {
        std::cerr << "gen tiling failed (profile=" << launch_profile::Name(cfg.profile) << ")" << std::endl;
        std::exit(1);
    }
    tilingData.set_stepM(1);
    tilingData.set_stepN(1);

    uint32_t tcubeTilingSize = tilingData.GetDataSize();
    tilingData.SaveToBuffer(tilingBuf, tcubeTilingSize);

    uint64_t localMemSize = 0;
    ascendcPlatform->GetCoreMemSize(platform_ascendc::CoreMemType::UB, localMemSize);
    *reinterpret_cast<uint64_t *>(tilingBuf + tcubeTilingSize) = localMemSize;

    std::cerr << "[tiling] profile=" << launch_profile::Name(cfg.profile)
              << " usedCoreNum=" << cfg.usedCoreNum
              << " singleCoreM=" << tilingData.get_singleCoreM()
              << " singleCoreN=" << tilingData.get_singleCoreN()
              << " singleCoreK=" << tilingData.get_singleCoreK() << std::endl;
}
