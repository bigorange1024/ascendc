/**
 * @file launch_profile.h
 * Stage2 Cube launch：aicore=1 / aicore=4（与 tiling 闭合参数一一对应）。
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

inline Profile FromEnv()
{
    return Parse(std::getenv("LAUNCH_PROFILE"));
}

/** M=16, N=512, K=256：aicore=1 → (16,256)×2；aicore=4 → (16,64)×8。 */
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

inline const char *Name(Profile p)
{
    return p == Profile::k4Aic ? "aicore=4" : "aicore=1";
}

} // namespace launch_profile

#endif
