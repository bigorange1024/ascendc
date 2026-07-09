# STATUS — exp-fips203-mlkem-pke-keygen-k4

**已晋级 stable**（2026-06-29）：[`stable-fips203-mlkem-pke-keygen-k4`](../../stable/stable-fips203-mlkem-pke-keygen-k4/) — 交付验收以 stable 为准（stable 2026-06-29：CPU/SIM/KAT ✅，SIM tick 542393）。

**语义**：FIPS 203 **Alg.13 全链 KeyGen**（k=4，ML-KEM-768 PKE）— **自包含交付示例**，**唯一向量化全链路径**（与 pass 探针终态对齐）。

**实现方案**：[`exp-fips203-mlkem-pke-keygen-k4-实现方案-customspec.pdf`](exp-fips203-mlkem-pke-keygen-k4-实现方案-customspec.pdf)

## 唯一交付路径

本目录**没有** G0–G4 门禁、分段 launch、标量回退或磁盘 staging 等多条验收路径。交付物只有：

| 项 | 内容 |
|----|------|
| **入口** | `bash run.sh` → `ascendc_keygen_bbit`（`main_keygen.cpp`） |
| **Launch** | ① `f203_keygen_prep`（AIV×2，**双 AIV 并行 Â**）② `compute/mmad_custom`（MIX 1AIC+2AIV，`F203_KEYGEN_EK_PKE=1`） |
| **I/O** | `input/` seed+LUT → `output/` ek_pke + dk_pke |
| **向量配置** | `run.sh` 锁定 prep 双 AIV + compute 全向量（Alg11/ByteEncode12/HAT 等，见实现方案 §唯一路径） |

可选：`KEYGEN_VERIFY=1`（Host golden 对拍 ek/dk）、`KEYGEN_DEBUG_DUMP=1`（debug 中间量）、`kat_liboqs_vs_ascendc.sh`（liboqs PKE KeyGen KAT）。

## 自包含约束

| 允许 | 禁止 |
|------|------|
| 本目录 `prep/`、`compute/`、`scripts/prep/`、`scripts/compute/` | `ascendc-tests/` 探针 include/import |
| `library/shared/`（SHAKE、Keccak） | 外链 vec-k4-v2、merged_kyber、分段 CMake |

## 验收

默认 **无需** 手动 `export SIM_DIRECT` / `HAT_*`（`run.sh -r sim` 内已设置全量生产路径）。

```bash
cd examples/incubating/exp-fips203-mlkem-pke-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh   # CPU×10 + SIM×1 vs liboqs
```

## Launch 与 AI Core

| Launch | 内核 | blockDim | 核类型 |
|--------|------|----------|--------|
| 1 prep | `f203_keygen_prep` | 2 | AIV_ONLY（双 AIV 并行 Â + block0 PRF/CBD） |
| 2 compute | `compute/mmad_custom` | 1 | MIX 1AIC+2AIV |

串行 **1 颗 AI Core**；**以 SIM `profile_subtask_log*.toml` 为准**（CPU `[SUCCESS][AIC_*]` 为 tikicpu artifact）。

**典型 SIM**（Ascend910B4）：prep ≈462161 + mmad ≈80475 → **total_tick ≈542393**。

**技术总结**：[docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md](../../../docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)、[docs/notes/F203-KeyGen-exp交付示例技术总结.md](../../../docs/notes/F203-KeyGen-exp交付示例技术总结.md)
