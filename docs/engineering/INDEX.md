# docs/engineering — 工程与环境

工具链、复现、构建约定、Agent 操作清单等。

| 文件 | 何时阅读 |
|------|----------|
| [环境复现与开发指南.md](环境复现与开发指南.md) | 新机器 / 新 Agent：CANN 9.0、WSL 无 NPU、`add_custom` 跑通；§12 Prompt、§13 清单、§14 测试矩阵 |
| [thirdparty-本地依赖.md](thirdparty-本地依赖.md) | `thirdparty/` 不进 Git；仓清单 URL + `clone-thirdparty.sh`（**默认 build liboqs**）；禁止 Agent 误删 |
| [用例自包含与设备全链约束.md](用例自包含与设备全链约束.md) | 新建/晋级探针或 example；禁止跨用例源码依赖、Host 辅助密码计算 |
| [内核计算超时与性能定标.md](内核计算超时与性能定标.md) | `KERNEL_COMPUTE_BUDGET_SEC` 防挂死 vs ML-KEM NTT 全流程 SIM ~15s 性能定标 |
| [Cloud-Agent额度与验收分层.md](Cloud-Agent额度与验收分层.md) | Cloud 额度经验（512 案例）；**轻验/重验**；他工程省额度；**不**降低本仓出口门禁 |
| [NPU真机环境说明.md](NPU真机环境说明.md) | WSL / Cloud / 真机对照；`scripts/runtime_env.sh`；`-r auto` / `-r verify`；WSL 禁 npu；探针接入范围；**§3.1 只读体检**、**§3.2 借入机体检回填（9.1.0-beta.3 / aarch64）**、**§4 上板适配清单**；**§4.1/`MSPROF_MODE=app` 多 launch 测准**、**§4.6 教材 14 档** |
| [实机一次搬码验收清单.md](实机一次搬码验收清单.md) | 借入 NPU **一条命令测全**：`npu_kem_one_trip.sh` + `BRING_BACK.tar.gz`；E1 trace 在教材 Encaps 之前 |
| [../notes/AscendC多环境运行纪要.md](../notes/AscendC多环境运行纪要.md) | 多环境**原理纪要**（不变量、Clang/`sim_env`、门禁纪律） |

---

## 维护

新增工程文档 → 在本表登记。
