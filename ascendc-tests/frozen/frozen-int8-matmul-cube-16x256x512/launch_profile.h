#ifndef LAUNCH_PROFILE_H
#define LAUNCH_PROFILE_H

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace launch_profile {

/**
 * Kyber Stage2 原生 C[16,512]=A[16,256]xB[256,512] 的 Matmul launch 闭合参数。
 *
 * k1Aic（默认，LAUNCH_PROFILE=aicore=1 或未设置）— f203 风格「1 个 launch block」：
 *   blockDim=1, SetDim(2), SetSingleShape(16,256,K)
 *   N 方向 2 切分（512/256=2）；CPU 扫参 PASS。
 *
 * k2Aic（LAUNCH_PROFILE=aicore=2）— 「2 个 launch block」：
 *   blockDim=2, SetDim(4), SetSingleShape(16,128,K)
 *   N 方向 4 切分（512/128=4）；CPU 扫参 PASS。
 *   迁移：export LAUNCH_PROFILE=aicore=2 && bash run.sh ...
 *
 * 反例（勿用）：blockDim=2, SetDim(2), single=(16,256) → CPU TIMEOUT。
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
