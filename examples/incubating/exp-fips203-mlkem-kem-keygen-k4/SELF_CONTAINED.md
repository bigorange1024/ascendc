# SELF_CONTAINED — exp-fips203-mlkem-kem-keygen-k4

本目录为 **自包含** Alg.19 KEM KeyGen incubating 实现（customspec 强制）。

| 内容 | 说明 |
|------|------|
| PKE 主体 | vendored：`prep/`、`compute/`、`f203_keygen_prep_*`（源自 stable PKE KeyGen 快照） |
| KEM 尾 | `kem/f203_kem_kg_*.hpp`；`F203_KEM_KEYGEN_TAIL=1` 挂在本地 `compute/mmad_custom.cpp` |
| Host | `main_kem_keygen.cpp`（**不是** `main_keygen.cpp`） |
| 构建 | `cmake/keygen/CMakeLists.txt` → 仅本树路径 |
| 规格 | `*-实现方案-customspec.{tex,pdf}` |
| 第三方 LUT | `thirdparty/ntt_onnx`（与仓 `thirdparty/` 对齐） |

**禁止**：`#include` / CMake 指向 `ascendc-tests/pass-fix-…-device-k4` 或 `frozen/`。
