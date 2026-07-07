# library/shared — 探针共用代码

| 路径 | 内容 |
|------|------|
| [`ascendc_build_mode.hpp`](ascendc_build_mode.hpp) | **全仓** `ASCENDC_BUILD_CPU`/`ASCENDC_BUILD_SIM` + `ASCENDC_SIM_HOST_MODE` 辅助 |
| [`f203_mod_q/`](f203_mod_q/) | Kyber q=3329 向量 Barrett mod 与模加（`mod_q_vec.hpp`、`mod_q_add.hpp`） |
| [`shake_xof_kernel/`](shake_xof_kernel/) | SHAKE128/256 XOF：`KernelShakeGeneral`（**LocalTensor I/O only**） |
| [`keccak_f1600_kernel/`](keccak_f1600_kernel/) | Keccak-f[1600] 置换（device header-only）+ **`fips203_device_sha3.hpp`**（AI Core 标量 SHA3-256/512、SHAKE256；语义对齐 tiny_sha3） |
| [`fips203_se_sample/`](fips203_se_sample/) | FIPS 203 采样参考 C + **`golden_se_sampling.py`**（`SEED_D`→`src` Host golden） |
| [`merged_kyber_fixed_poly.py`](merged_kyber_fixed_poly.py) | Kyber 固定 poly 脚本 |
| [`stage2_debug_print.hpp`](stage2_debug_print.hpp) | Stage2 调试打印 |
| [`compare_stage2_logs.py`](compare_stage2_logs.py) | Stage2 log 对比 |

## shake_xof_kernel API

- **`KernelShakeGeneral::Init(x, lengths, y, staging32, tiling*)`** — 全部 UB；`staging32` ≥ `SHAKE_XOF_STAGING_BYTES`（32B）
- **`shake_ub_helpers.hpp`**：`FillShakeTilingUb`、`FillShakeRowUb`、`RunKernelShakeGeneralUb`（内嵌调用走 **`ProcessInline`**，不受外层 `blockDim` 影响；独立多核 launch 用 `Process()`）
- 块 I/O：32B 用 **uint64 块**（4×/32B）；SIM 上不用 UB→UB `DataCopy` 写 y（见 qa/2026-06-23 §15.1）
- **禁止**在 shake 热路径使用 GM 搬运 x/y；集成用例单 TPipe 直连 UB。
- **toy 参考**：各 `pass-shake*-toy/*_toy_ub.hpp`（UB 自检 + `auto_gen/toy_active_case.h`），非共享库内 GM 桥接。
