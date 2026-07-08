/**
 * @file byte_encode12_tiling.cpp
 * @brief ByteEncode₁₂-only 探针运行时 tiling 生成（模板风格，替代 Python input/tiling.bin）。
 *
 * 对齐 AscendC 样例（matmul_custom_tiling.cpp / sepolyvec8_ntt_custom_tiling.cpp）：tiling 由宿主端
 * 专门 .cpp 运行时用 C++ 生成，数值集中于此便于修改；不再由 gen_data.py 的 struct.pack 落盘。
 * device 编译期几何仍是 tiling.h 的 `namespace tiling` constexpr。
 */
#include "tiling.h"

/**
 * 填充运行时 TilingData（本探针仅一个字段）。
 * @param data 输出 tiling；tileLength = tiling::n = 256，与旧 `struct.pack("<i", N)` 等价。
 */
void GenerateTiling(TilingData &data)
{
    data.tileLength = static_cast<int32_t>(tiling::n);
}
