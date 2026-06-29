#ifndef LAUNCH_PROFILE_H
#define LAUNCH_PROFILE_H

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace launch_profile {

/**
 * Stage2 Matmul C[16,512]=A[16,256]xB[256,512] launch 参数（与 int8-matmul-cube-16x256x512 一致）。
 *
 * k1Aic（默认）— f203 风格：blockDim=1, SetDim(2), single=(16,256)
 * k2Aic — 2 launch block：blockDim=2, SetDim(4), single=(16,128)
 *
 * Stage1 MIX Split 始终 blockDim=1（同 block-s123），与 profile 无关。
 */
enum class Profile { k1Aic, k2Aic };

struct Config {
    Profile profile;
    uint32_t blockDim;
    int32_t usedCoreNum;
    int32_t singleCoreM;
    int32_t singleCoreN;
};

inline Profile Parse(const char *name)
{
    if (name == nullptr || name[0] == '\0') {
        return Profile::k1Aic;
    }
    if (strncmp(name, "aicore=", 7) == 0) {
        const int n = atoi(name + 7);
        if (n == 2) {
            return Profile::k2Aic;
        }
    }
    return Profile::k1Aic;
}

inline Profile FromEnv()
{
    return Parse(std::getenv("LAUNCH_PROFILE"));
}

inline Config Get(Profile p)
{
    switch (p) {
    case Profile::k2Aic:
        return {Profile::k2Aic, 2, 4, 16, 128};
    case Profile::k1Aic:
    default:
        return {Profile::k1Aic, 1, 2, 16, 256};
    }
}

inline const char *Name(Profile p)
{
    return p == Profile::k2Aic ? "aicore=2" : "aicore=1";
}

} // namespace launch_profile

#endif
