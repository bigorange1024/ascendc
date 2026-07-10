/**
 * @file launch_profile.h
 * @brief sepolyvec8 Stage2 Cube tiling 剖面：aicore=1 / aicore=4。
 *
 * 流水线位置：sepolyvec8_ntt_custom_tiling.cpp 读 LAUNCH_PROFILE，设置 usedCoreNum/singleCore。
 * 与 golden：仅影响 Cube 分核，不改变 NTT I/O 语义。
 */
#ifndef LAUNCH_PROFILE_H
#define LAUNCH_PROFILE_H

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace launch_profile {

enum class Profile { k1Aic, k4Aic };

struct Config {
    Profile profile;
    uint32_t blockDim;
    int32_t usedCoreNum;
    bool useSetSingleShape;
    int32_t singleCoreM;
    int32_t singleCoreN;
};

/** 解析 "aicore=N"；非 4 则回落 aicore=1。 */
inline Profile Parse(const char *name)
{
    if (name != nullptr && strncmp(name, "aicore=", 7) == 0) {
        const int n = atoi(name + 7);
        if (n == 4) {
            return Profile::k4Aic;
        }
    }
    return Profile::k1Aic;
}

/** 读环境变量 LAUNCH_PROFILE。 */
inline Profile FromEnv()
{
    return Parse(std::getenv("LAUNCH_PROFILE"));
}

/**
 * 剖面 → Cube tiling 参数。
 * aicore=1：usedCoreNum=2, single 16x256；aicore=4：usedCoreNum=8, single 16x64。
 */
inline Config Get(Profile p)
{
    switch (p) {
    case Profile::k4Aic:
        return {Profile::k4Aic, 4, 8, true, 16, 64};
    case Profile::k1Aic:
    default:
        return {Profile::k1Aic, 1, 2, true, 16, 256};
    }
}

/** 剖面可读名。 */
inline const char *Name(Profile p)
{
    return p == Profile::k4Aic ? "aicore=4" : "aicore=1";
}

} // namespace launch_profile

#endif
