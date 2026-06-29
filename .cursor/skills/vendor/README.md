# 昇腾官方 Agent Skills（精选 vendored）

来源：[Ascend/agent-skills](https://github.com/Ascend/agent-skills)（`master` 浅克隆拷贝）。

**用途**：CANN 环境、设备查询、NPU profiling 等**外围**参考；**不**替代本仓 `ascendc-engineering-notes`（KernelLaunch / FIPS 203·204 路线）。

| 目录 | 说明 |
|------|------|
| `cann-operator-env-config` | CANN 安装与环境变量 |
| `npu-smi` | `npu-smi` 命令参考 |
| `ascend-profiling-anomaly` | Profiling 异常与瓶颈分析 |

**未安装**（易与本仓 KernelLaunch 探针冲突）：`ascendc-operator-dev`、`ascendc-operator-project-init`、`ascendc-operator-code-gen` 等 ascend-kernel 全流程。

更新：重新 `git clone --depth 1` 后覆盖对应子目录。
