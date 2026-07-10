# library/shared — 探针共用代码

| 路径 | 内容 |
|------|------|
| [`ascendc_build_mode.hpp`](ascendc_build_mode.hpp) | **全仓强制** CPU/SIM 与 SIM host 选项入口（见下） |
| [`f203_mod_q/`](f203_mod_q/) | Kyber q=3329 向量 Barrett mod 与模加（`mod_q_vec.hpp`、`mod_q_add.hpp`） |
| [`f203_unified_round/`](f203_unified_round/) | FIPS 203 **统一整数** Compress/Decompress 设备头（`C=41285357`；Compress **int32 limb** 向量、Decompress int32 向量）；探针见 [`exp-fips203-compress-unified-int-vec-k4`](../../examples/incubating/exp-fips203-compress-unified-int-vec-k4/) / [`exp-fips203-decompress-unified-int-vec-k4`](../../examples/incubating/exp-fips203-decompress-unified-int-vec-k4/) |
| [`f203_byte_codec/`](f203_byte_codec/) | ByteDecode₁₂ 等设备侧编解码辅助 |
| [`shake_xof_kernel/`](shake_xof_kernel/) | SHAKE128/256 XOF：`KernelShakeGeneral`（**LocalTensor I/O only**） |
| [`keccak_f1600_kernel/`](keccak_f1600_kernel/) | Keccak-f[1600] 置换（device header-only）+ **`fips203_device_sha3.hpp`**（AI Core 标量 SHA3-256/512、SHAKE256；语义对齐 tiny_sha3） |
| [`fips203_se_sample/`](fips203_se_sample/) | FIPS 203 采样参考 C + **`golden_se_sampling.py`**（`SEED_D`→`src` Host golden） |
| [`merged_kyber_fixed_poly.py`](merged_kyber_fixed_poly.py) | Kyber 固定 poly 脚本 |
| [`stage2_debug_print.hpp`](stage2_debug_print.hpp) | Stage2 调试打印 |

## `ascendc_build_mode.hpp` — 全仓编译 / SIM host 选项（强制）

**定稿**：[docs/notes/AscendC-CPU与SIM实现分叉开发指南.md](../../docs/notes/AscendC-CPU与SIM实现分叉开发指南.md)

| 类型 | 写法 | 禁止 |
|------|------|------|
| **平台（编译期）** | `#include "ascendc_build_mode.hpp"` → `ASCENDC_BUILD_CPU` / `ASCENDC_BUILD_SIM` | per-probe 编译宏分叉 host |
| **SIM host 拓扑（运行时）** | `ASCENDC_SIM_HOST_MODE=<登记取值>`；main 用 `ascendc::SimHostModeIs` 或 helper | `F203_FEAS_*`、`KEM_DECAPS_SIM_2SESSION` 等新 env |
| **算法变体** | CMake `CACHE` + `target_compile_definitions`（CPU/SIM 同值） | 用 env 在 CPU/SIM 切 launch |

**已登记 SIM host 取值**（§3.3）：`phased_launch`（encrypt-compute）· `decaps_2session` / `decaps_1session`（kem-decaps）。  
**新探针**：在指南 §3.3 追加一行后再写 `run.sh` 默认 export。

**CMake**：host 需 `#include` 本头时，在对应 `CMakeLists.txt` 增加 `${REPO_ROOT}/library/shared` 到 include 路径。

## shake_xof_kernel API

- **`KernelShakeGeneral::Init(x, lengths, y, staging32, tiling*)`** — 全部 UB；`staging32` ≥ `SHAKE_XOF_STAGING_BYTES`（32B）
- **`shake_ub_helpers.hpp`**：`FillShakeTilingUb`、`FillShakeRowUb`、`RunKernelShakeGeneralUb`（内嵌调用走 **`ProcessInline`**，不受外层 `blockDim` 影响；独立多核 launch 用 `Process()`）
- 块 I/O：32B 用 **uint64 块**（4×/32B）；SIM 上不用 UB→UB `DataCopy` 写 y（见 qa/2026-06-23 §15.1）
- **禁止**在 shake 热路径使用 GM 搬运 x/y；集成用例单 TPipe 直连 UB。
- **toy 参考**：各 `pass-shake*-toy/*_toy_ub.hpp`（UB 自检 + `auto_gen/toy_active_case.h`），非共享库内 GM 桥接。
