#pragma once

/**
 * @file ascendc_build_mode.hpp
 * @brief 全仓统一的 CPU / SIM 编译期宏与 SIM host 编排 env（library/shared）。
 *
 * 编译期（CMake RUN_MODE=cpu 时定义 ASCENDC_CPU_DEBUG，本头派生 BUILD_*）：
 *   ASCENDC_BUILD_CPU  — tikicpu 孪生（main 用 ICPU_RUN_KF）
 *   ASCENDC_BUILD_SIM  — CaModel / NPU（main 用 ACLRT_LAUNCH_KERNEL）
 *
 * 运行时（仅 ASCENDC_BUILD_SIM 的 host/main 可读，kernel 禁止 getenv 切 launch）：
 *   ASCENDC_SIM_HOST_MODE=<mode>  — 见 docs/notes/AscendC-CPU与SIM实现分叉开发指南.md §3.3 登记表
 *
 * 禁止：每用例自建 F203_FEAS_* / KEM_*_SIM_* 等分叉 env（须登记到 ASCENDC_SIM_HOST_MODE 取值表）。
 */
#include <cstring>
#include <cstdlib>

#if defined(ASCENDC_CPU_DEBUG)
#define ASCENDC_BUILD_CPU 1
#define ASCENDC_BUILD_SIM 0
#else
#define ASCENDC_BUILD_CPU 0
#define ASCENDC_BUILD_SIM 1
#endif

namespace ascendc {

#if ASCENDC_BUILD_CPU
constexpr bool kBuildIsCpu = true;
constexpr bool kBuildIsSim = false;
#else
constexpr bool kBuildIsCpu = false;
constexpr bool kBuildIsSim = true;
#endif

/** SIM host：当前 mode 是否与 name 相等；CPU 编译恒 false。 */
inline bool SimHostModeIs(const char *name)
{
#if ASCENDC_BUILD_CPU
    (void)name;
    return false;
#else
    if (name == nullptr) {
        return false;
    }
    const char *mode = std::getenv("ASCENDC_SIM_HOST_MODE");
    return mode != nullptr && std::strcmp(mode, name) == 0;
#endif
}

/** SIM host：读取 ASCENDC_SIM_HOST_MODE；CPU 编译恒 nullptr。 */
inline const char *SimHostModeOrNull()
{
#if ASCENDC_BUILD_CPU
    return nullptr;
#else
    return std::getenv("ASCENDC_SIM_HOST_MODE");
#endif
}

/** encrypt-compute：默认 fused 单 launch；phased_launch 为 3 launch 调试。 */
inline bool SimHostEncryptFeasPhasedLaunch()
{
#if ASCENDC_BUILD_CPU
    return false;
#else
    return SimHostModeIs("phased_launch");
#endif
}

/**
 * decaps：默认 decaps_2session（生产 SIM）；decaps_1session 为排障。
 * 兼容旧 env KEM_DECAPS_SIM_2SESSION=0/1（deprecated，run.sh 应写 ASCENDC_SIM_HOST_MODE）。
 */
inline bool SimHostDecapsUse2Session()
{
#if ASCENDC_BUILD_CPU
    return false;
#else
    if (SimHostModeIs("decaps_1session")) {
        return false;
    }
    if (SimHostModeIs("decaps_2session")) {
        return true;
    }
    const char *legacy = std::getenv("KEM_DECAPS_SIM_2SESSION");
    if (legacy != nullptr) {
        return legacy[0] == '1';
    }
    return true;
#endif
}

} // namespace ascendc
