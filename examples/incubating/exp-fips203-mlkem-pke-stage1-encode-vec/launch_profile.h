/**
 * @file launch_profile.h
 * @brief Stage1 Vector launch 剖面：aiv=1 / aiv=2 / aiv=8。
 *
 * 流水线位置：Host main 读 LAUNCH_PROFILE，决定 blockDim 与每核 poly 数。
 * - aiv=1：单核串行处理 8 条 poly
 * - aiv=2：每核 4 poly（偶数 AIV 最小多核，对齐融合算子 customspec）
 * - aiv=8：每核 1 poly（blockIdx = poly 下标）
 * run.sh 经 --aiv N 设置环境变量 LAUNCH_PROFILE=aiv=N。
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

/**
 * 解析 "aiv=N" 字符串为 Profile；非法/空则回落 aiv=1。
 * @param name 环境变量或命令行剖面名
 */
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

/** 从环境变量 LAUNCH_PROFILE 读取剖面。 */
inline Profile FromEnv()
{
    return Parse(std::getenv("LAUNCH_PROFILE"));
}

/**
 * 剖面 → Config：blockDim 即 AIV 核数。
 * @param p 已解析剖面
 */
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

/** 剖面可读名（日志用）。 */
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
