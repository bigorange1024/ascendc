#ifndef MATMUL_SHAPE_H
#define MATMUL_SHAPE_H

#include <cstdlib>

namespace matmul_shape {

inline int EnvInt(const char *name, int defaultVal)
{
    const char *v = std::getenv(name);
    if (v == nullptr || v[0] == '\0') {
        return defaultVal;
    }
    return std::atoi(v);
}

inline int M()
{
    return EnvInt("MATMUL_M", 128);
}
inline int N()
{
    return EnvInt("MATMUL_N", 512);
}
inline int K()
{
    return EnvInt("MATMUL_K", 512);
}
inline int UsedCoreNum()
{
    return EnvInt("MATMUL_USED_CORE_NUM", 32);
}
inline int BlockDim()
{
    return EnvInt("MATMUL_BLOCK_DIM", 16);
}
inline int SingleCoreM()
{
    return EnvInt("MATMUL_SINGLE_M", 16);
}
inline int SingleCoreN()
{
    return EnvInt("MATMUL_SINGLE_N", 128);
}

} // namespace matmul_shape

#endif
