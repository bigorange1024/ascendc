# STATUS — exp-mlkem-f203-pke-keygen-k4

**语义**：FIPS 203 **Alg.13 全链 KeyGen**（k=4，ML-KEM-768 PKE）— **自包含交付示例**，**唯一向量化全链路径**。

**实现方案**：[`exp-mlkem-f203-pke-keygen-k4-实现方案-customspec.tex`](exp-mlkem-f203-pke-keygen-k4-实现方案-customspec.tex)

## 唯一交付路径

本目录**没有** G0–G4 门禁、分段 launch、标量回退或磁盘 staging 等多条验收路径。交付物只有：

| 项 | 内容 |
|----|------|
| **入口** | `bash run.sh` → `ascendc_keygen_bbit`（`main_keygen.cpp`） |
| **Launch** | ① `f203_keygen_prep`（AIV×2）② `compute/mmad_custom`（MIX 1AIC+2AIV，`F203_KEYGEN_EK_PKE=1`） |
| **I/O** | `input/` seed+LUT → `output/` ek_pke + dk_pke |
| **向量配置** | `run.sh` 锁定 prep 双 AIV + compute 全向量（Alg11/ByteEncode12/HAT 等，见实现方案 §唯一路径） |

可选：`KEYGEN_VERIFY=1`（Host golden 对拍 ek/dk）、`KEYGEN_DEBUG_DUMP=1`（debug 中间量）、`kat_liboqs_vs_ascendc.sh`（外部 liboqs 对照）。

## 自包含约束

| 允许 | 禁止 |
|------|------|
| 本目录 `prep/`、`compute/`、`scripts/prep/`、`scripts/compute/` | `ascendc-tests/` 探针 include/import |
| `library/shared/`（SHAKE、Keccak） | 外链 vec-k4-v2、merged_kyber、分段 CMake |

## 验收

```bash
cd examples/incubating/exp-mlkem-f203-pke-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh   # 可选：10×CPU + 1×SIM vs liboqs
```

## Launch 与 AI Core

| Launch | 内核 | blockDim | 核类型 |
|--------|------|----------|--------|
| 1 prep | `f203_keygen_prep` | 2 | AIV_ONLY |
| 2 compute | `compute/mmad_custom` | 1 | MIX 1AIC+2AIV |

串行 **1 颗 AI Core**；SIM tick 摘要见 `run.sh -r sim` 控制台输出。

**典型 SIM**（Ascend910B4）：Launch1 prep ≈801491 + Launch2 compute ≈83041 → **total_tick ≈884532**。

**教学总结**：[docs/notes/F203-KeyGen-exp交付示例技术总结.md](../../../docs/notes/F203-KeyGen-exp交付示例技术总结.md)
