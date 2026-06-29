# STATUS — pass-fix-f203-alg13-device-keygen-k4

**语义**：FIPS 203 **Alg.13 全链 KeyGen**（k=4，ML-KEM-768 PKE）— **2 次 device launch** + **行 21 `ek‖ρ` 在 compute 内核内融合**。

**实现方案 PDF**：[`pass-fix-f203-alg13-device-keygen-k4-实现方案-customspec.tex`](pass-fix-f203-alg13-device-keygen-k4-实现方案-customspec.tex)（`bash ../../scripts/xelatex-clean.sh …`）

## 生产 I/O（与 `exp-mlkem-f203-pke-keygen-k4` 一致）

| 目录 | 内容 |
|------|------|
| `input/` | `seed_d.bin` + `lut_even/odd_stacked.bin`（`scripts/prepare_production_input.py`） |
| `output/` | `ek_pke.bin` (1568B) + `dk_pke.bin` (1536B) |

中间 GM **不落盘**；`KEYGEN_DEBUG_DUMP=1` → `output/debug/`；`KEYGEN_VERIFY=1` → Host golden 对拍 ek/dk（不写 stray input）。

## 验收

| 项 | CPU | SIM | 说明 |
|----|-----|-----|------|
| 生产全链 | ✅ | ✅ | `bash run.sh` → `ascendc_keygen_bbit` |
| liboqs KAT | ✅ | ✅ | `bash kat_liboqs_vs_ascendc.sh` |
| Host golden | ✅ | — | `KEYGEN_VERIFY=1 bash run.sh -r cpu` |

```bash
cd ascendc-tests/pass-fix-f203-alg13-device-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh
```

**SIM 基线（2026-06-28，910B4）**：Total tick **886801**（prep ~806k + mmad ~81k，单进程 2 launch）。

## Launch 与 AI Core

| Launch | 内核 | blockDim | 核类型 | SIM 实际 |
|--------|------|----------|--------|----------|
| 1 prep | `f203_keygen_prep` | 2 | AIV_ONLY | 0 AIC；2 AIV block（block0 重活） |
| 2 compute | `mmad_custom` | 1 | MIX_AIC_1_2 | 1 AIC + 2 AIV |

串行占用 **1 颗 AI Core**（非双核并行）。CPU `[SUCCESS][AIC_x]` 为 tikicpu 拓扑 artifact；以 `sim_log/profile_*.toml` 为准。

## 源码注释

全目录 120+ 源文件已注入 `@probe` 文件头（`scripts/inject_probe_code_comments.py`）。入口：`main_keygen.cpp`、`run.sh`、`f203_keygen_prep_ub.hpp`、`compute/mmad_custom.cpp`。

## 遗留（非默认 `run.sh`）

- `cmake/prep`、`cmake/compute` + `main_keygen_prep.cpp` / `main_compute.cpp`：分段调试，**磁盘 staging，非生产 I/O**。
- G1 `f203_keygen_ek_append`：历史回归，禁止接入全链。

**文档**：[`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) · [`BUILD_OPTIONS.md`](BUILD_OPTIONS.md) · [`PIPE_SYNC_EVAL.md`](PIPE_SYNC_EVAL.md)
