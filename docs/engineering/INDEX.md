# docs/engineering — 工程与环境

工具链、复现、构建约定、Agent 操作清单等。

| 文件 | 何时阅读 |
|------|----------|
| [环境复现与开发指南.md](环境复现与开发指南.md) | 新机器 / 新 Agent：CANN 9.0、WSL 无 NPU、`add_custom` 跑通；§12 Prompt、§13 清单、§14 测试矩阵 |
| [thirdparty-本地依赖.md](thirdparty-本地依赖.md) | `thirdparty/` 不进 Git；六仓 URL + `clone-thirdparty.sh`（**默认 build liboqs**）；禁止 Agent 误删 |
| [用例自包含与设备全链约束.md](用例自包含与设备全链约束.md) | 新建/晋级探针或 example；禁止跨用例源码依赖、Host 辅助密码计算 |
| [内核计算超时与性能定标.md](内核计算超时与性能定标.md) | `KERNEL_COMPUTE_BUDGET_SEC` 防挂死 vs ML-KEM NTT 全流程 SIM ~15s 性能定标 |
| [NPU真机环境说明.md](NPU真机环境说明.md) | WSL / Cloud / 真机对照；`scripts/runtime_env.sh`；`-r auto` / `-r verify`；WSL 禁 npu |

---

## 维护

新增工程文档 → 在本表登记。
