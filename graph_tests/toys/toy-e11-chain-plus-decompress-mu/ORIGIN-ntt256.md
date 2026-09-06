# ORIGIN — pass-merged-kyber-mix-ntt256

## 授权与迁入（2026-07-13）

| 项 | 内容 |
|----|------|
| 上游仓库 | https://github.com/serial2007/MmadBiasInvocation1.git |
| 本机历史路径 | `thirdparty/merged_kyber/`（已删除，不再作为 thirdparty 依赖） |
| 授权 | 作者已授权本工程将本实现作为 **ascendc-tests 用例** 保留与演进 |
| 迁入内容 | kernel/host/cmake/scripts（`ntt_sim_kyber` golden）；**未**迁入构建产物、`merged-kyber.pptx`、`.git` |

## 工程适配（相对上游）

- 默认 `SOC_VERSION=Ascend910B4`；`run.sh` 接入本仓 `env.sh` / `sim_env.sh` / `camodel_sim_log.sh` / `kernel-run-timeout.sh`
- CMake：CPU 定义 `ASCENDC_CPU_DEBUG`；SIM rpath；链接 `unified_dlog`（对齐本仓 KernelLaunch 探针）
- 文档：本仓 `STATUS.md` 为准；上游 README 中 Matmul+bias 表格为更早样例描述，**以当前 MIX NTT 源码为准**

## 引用约定

其他活跃探针若曾 `MERGED_KYBER_ROOT=thirdparty/merged_kyber`：应改为本目录自包含头文件，或仅引用本用例的 `scripts/ntt_sim_kyber.py`（优先将脚本 vendored 进各自 `scripts/`，避免跨探针 `#include`）。
