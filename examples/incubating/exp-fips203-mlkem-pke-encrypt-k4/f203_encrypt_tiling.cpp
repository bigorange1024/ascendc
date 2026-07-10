/**
 * @file f203_encrypt_tiling.cpp
 * @brief Alg.14 encrypt compute+tail 运行时 tiling 生成（模板风格，替代 Python input/tiling.bin）。
 *
 * 采用 AscendC 样例（operator/ascendc/tutorials/.../matmul_custom_tiling.cpp、
 * examples/incubating/exp-sepolyvec8-ntt-k8/sepolyvec8_ntt_custom_tiling.cpp）的做法：
 * tiling 由宿主端专门的 .cpp 在**运行时用 C++ 生成**，可调数值集中于本文件的字面量，
 * 便于直接改动；不再依赖 gen_data.py 的 struct.pack 落盘。
 *
 * 说明：本探针 device 侧的**编译期几何**（workspace 偏移 / LUT 布局 / 分核 / INTT k=8 几何）
 * 仍是 compute/f203_l18_l19_tiling.h 的 `namespace tiling` constexpr（kernel 直接取值）。
 * 本文件只负责 **运行时 TilingData 三标量**，与旧 Python `struct.pack("<iii", N, K, 3)` 逐值等价。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024（k=4）K-PKE.Encrypt；本文件属 exp-fips203-mlkem-pke-encrypt-k4。
 * 与 golden：最终对拍 output/c.bin（中间态默认不落盘）。
 */
#include "f203_l18_l19_tiling.h"

/**
 * 填充运行时 TilingData（compute/tail 各 launch 共用；随 kernel 入参下发到 device）。
 *
 * @param data 输出 tiling 结构（tileLength / kPolys / mixPass）。
 *
 * 数值来源（与旧 tiling.bin 等价）：
 *   tileLength = tiling::n  = 256  —— 单 poly 系数个数 n
 *   kPolys     = tiling::kK = 4    —— NTT(y) / Â 列 poly 数 k
 *   mixPass    = 3                 —— 可行性核固定全链（S1+S2+S3 + 行18/19–21）
 */
void GenerateTiling(TilingData &data)
{
    data.tileLength = static_cast<int32_t>(tiling::n);
    data.kPolys = static_cast<int32_t>(tiling::kK);
    data.mixPass = 3;
}
