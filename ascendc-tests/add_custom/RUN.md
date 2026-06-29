# Add 算子示例（CANN 9.0）

向量加法 `z = x + y`，数据量 `8 × 2048`，`float16`。

## 运行模式

| 模式 | 命令 | 说明 |
|------|------|------|
| CPU 孪生 | `./run.sh cpu 910B4` | 无 NPU，逻辑 + golden 比对 |
| **仿真 + 性能（默认）** | `./run.sh sim 910B4` | **`msprof op simulator`** → `prof_sim/`、`OPPROF_*` |
| **仅仿真（无 OPPROF）** | `SIM_DIRECT=1 ./run.sh sim 910B4` 或 `./run.sh sim 910B4 SIM_DIRECT=1` | 只跑 CAModel + golden，不跑 msprof |
| 仅编译 | `./run.sh sim-build 910B4` | WSL 无卡时只验证编译链 |
| 实机上板 | `./run.sh npu 910B4` | ACL 真机运行 |
| 实机 + 性能 | `RUN_WITH_MSPROF=1 ./run.sh npu 910B4` | **`msprof op`** 采实机性能 |

## 为什么没有性能数据？

默认 `./run.sh sim` 会调用 **`msprof`**，成功时生成 **`OPPROF_*`** 与 **`prof_sim/`**。

若只要 CAModel 验证算子、不要性能目录，设 **`SIM_DIRECT=1`**（参见 [算子调优 msProf 文档](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/devaids/optool/atlasopdev_16_0090.html)）：

```bash
source ~/ascendc/scripts/env.sh
cd ~/ascendc/ascendc-tests/add_custom
./run.sh sim 910B4                    # 默认：msprof + OPPROF_*
SIM_DIRECT=1 ./run.sh sim 910B4       # 仅仿真，无 OPPROF_*
# 或: ./run.sh sim 910B4 SIM_DIRECT=1
```

脚本会执行：

```bash
msprof op simulator \
  --application=./add_custom_npu \
  --soc-version=Ascend910B4 \
  --output=./prof_sim \
  --aic-metrics=PipeUtilization,ResourceConflictRatio
```

结果在 **`prof_sim/`**（如 `PipeUtilization.csv`、`trace.json` 等），CAModel 日志在 **`sim_log/`**。

## 环境变量

| 变量 | 含义 |
|------|------|
| `SIM_DIRECT` | **0**（默认）：`./run.sh sim` 走 msprof，生成 `OPPROF_*`；**1**：仅仿真跑算子，不生成 `OPPROF_*` |
| `MSPROF_AIC_METRICS_SIM` | sim 下 msprof 指标（默认 `PipeUtilization`） |
| `RUN_WITH_MSPROF=1` | npu 模式走 msprof |

| 变量 | sim 默认 | 实机 npu 默认 |
|------|----------|----------------|
| `MSPROF_AIC_METRICS_SIM` | `PipeUtilization,ResourceConflictRatio` | — |
| `MSPROF_AIC_METRICS_NPU` | — | `PipeUtilization,MemoryUB,Memory` |
| `MSPROF_LAUNCH_COUNT` | `8` | `8` |

**注意**：`msprof op simulator` **不支持** `MemoryUB`/`Memory`，否则会报 `Unexpected argument --aic-metrics=memoryub` 且采不到数据。

性能结果在 **`prof_sim/*.csv`**，终端会打印 `PipeUtilization.csv` 前几行摘要；完整数据用 MindStudio 打开 `prof_sim/`。

## 设备名

见 `scripts/resolve_device.sh`：`910B4` → `Ascend910B4` / `dav-c220`。

## WSL

- `./run.sh cpu` / `./run.sh sim-build`：无 NPU 可用  
- `./run.sh sim`：需本机已装 CANN 且 `msprof` 可用；无卡环境可能失败，请在昇腾实机执行
