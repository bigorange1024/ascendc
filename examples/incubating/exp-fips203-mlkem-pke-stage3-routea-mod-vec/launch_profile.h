/**
 * @file launch_profile.h
 * Stage3 Vector launch：aiv=1 / aiv=2 / aiv=8（与 Stage1 命名一致）。
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

inline Profile FromEnv()
{
    return Parse(std::getenv("LAUNCH_PROFILE"));
}

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
