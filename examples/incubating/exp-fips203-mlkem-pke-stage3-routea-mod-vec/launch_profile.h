/**
 * @file launch_profile.h
 * @brief Stage3 RouteA+mod Vector launch 剖面：aiv=1 / aiv=2 / aiv=8。
 *
 * 流水线位置：与 Stage1 同构分核；Host main 读 LAUNCH_PROFILE 得 blockDim。
 * - aiv=1：单核串行 8 poly；aiv=2：每核 4；aiv=8：每核 1。
 * 与 golden：仅影响并行度，不改变 I/O 语义。
 */
#ifndef LAUNCH_PROFILE_H
#define LAUNCH_PROFILE_H

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace launch_profile {

enum class Profile { k1Aiv, k2Aiv, k8Aiv };

struct Config {
    Profile profile;
    uint32_t blockDim;
};

/** 解析 "aiv=N"；非法回落 aiv=1。 */
inline Profile Parse(const char *name)
{
    if (name != nullptr && strncmp(name, "aiv=", 4) == 0) {
        const int n = atoi(name + 4);
        if (n == 8) {
            return Profile::k8Aiv;
        }
        if (n == 2) {
            return Profile::k2Aiv;
        }
    }
    return Profile::k1Aiv;
}

/** 读环境变量 LAUNCH_PROFILE。 */
inline Profile FromEnv()
{
    return Parse(std::getenv("LAUNCH_PROFILE"));
}

/** 剖面 → blockDim。 */
inline Config Get(Profile p)
{
    switch (p) {
    case Profile::k8Aiv:
        return {Profile::k8Aiv, 8};
    case Profile::k2Aiv:
        return {Profile::k2Aiv, 2};
    case Profile::k1Aiv:
    default:
        return {Profile::k1Aiv, 1};
    }
}

/** 剖面可读名。 */
inline const char *Name(Profile p)
{
    if (p == Profile::k8Aiv) {
        return "aiv=8";
    }
    if (p == Profile::k2Aiv) {
        return "aiv=2";
    }
    return "aiv=1";
}

} // namespace launch_profile

#endif
