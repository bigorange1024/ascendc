/**
 * @file f203_encrypt_tiling.cpp
 * @brief Alg.14 encrypt compute（行 2/18/19/21）运行时 tiling 生成（模板风格，替代 Python input/tiling.bin）。
 *
 * 采用 AscendC 样例（operator/ascendc/tutorials/.../matmul_custom_tiling.cpp、
 * examples/incubating/exp-sepolyvec8-ntt-k8/sepolyvec8_ntt_custom_tiling.cpp）的做法：
 * tiling 由宿主端专门 .cpp 在**运行时用 C++ 生成**，可调数值集中于本文件字面量，便于修改；
 * 不再依赖 gen_data.py 的 struct.pack 落盘。
 *
 * device 编译期几何（workspace 偏移 / LUT 布局 / 分核 / INTT k=8 几何）仍是
 * compute/f203_l18_l19_tiling.h 的 `namespace tiling` constexpr。本文件只负责运行时三标量，
 * 与旧 Python `struct.pack("<iii", N, K, 3)` 逐值等价。
 */
#include "f203_l18_l19_tiling.h"

/**
 * 填充运行时 TilingData（三 launch / 融合单 launch 共用；随 kernel 入参下发到 device）。
 *
 * @param data 输出 tiling 结构。
 * 数值：tileLength = tiling::n = 256；kPolys = tiling::kK = 4；mixPass = 3（固定全链）。
 */
void GenerateTiling(TilingData &data)
{
    data.tileLength = static_cast<int32_t>(tiling::n);
    data.kPolys = static_cast<int32_t>(tiling::kK);
    data.mixPass = 3;
}
