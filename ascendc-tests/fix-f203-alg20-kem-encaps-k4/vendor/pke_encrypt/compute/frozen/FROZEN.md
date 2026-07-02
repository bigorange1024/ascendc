# compute/frozen — 旧 G3 拆分核路线关闭

**前缀**：`frozen-<原名>/`（本探针内子目录冻结，非顶层 `ascendc-tests/frozen/`）

**判决日期**：2026-06-30

## 为何关闭

| 病根 | 说明 |
|------|------|
| R1 | SIM `device_aiv.o` 内 AIV-only `func_key ≥ 5` → `507000`；旧路线 4 核 + 多 session 必踩边界 |
| R2 | 多次 `aclInit/aclFinalize` + D2H 未 `aclrtSynchronizeStream` |

旧 G3 拆成 `g3_linear` / `g3_linear4` / `at_r` / `t_dot_r` 四个 AIV-only kernel，加上 SIM 上「两次 `at_r` 独立 session + col0 hack」绕行，**数学上可等价但工程上不可维护**。2026-06-30 本地独立验证 + `at_r5` 合并核落地后，本路线**永久关闭**。

## 冻结目录

| 目录 | 含 kernel | 关闭原因 |
|------|-----------|----------|
| [`frozen-g3_linear/`](frozen-g3_linear/) | `f203_encrypt_g3_linear` / `g3_linear4` / `at_r` / `t_dot_r`（单文件四入口） | 已从 `KERNEL_FILES` 移除；`g3_linear`/`g3_linear4` 五参/四参 launch 在 SIM `func_key≥5` 时 507000 |
| [`frozen-at_r/`](frozen-at_r/) | 独立 `f203_encrypt_at_r` 副本 | 与 `g3_linear.cpp` 重复；SIM 双 session 绕行产物 |
| [`frozen-t_dot_r/`](frozen-t_dot_r/) | 独立 `f203_encrypt_t_dot_r` | 误判「入口失效」；实为 `func_key=7` 踩 R1 |

## 合法继任

| 能力 | 继任 |
|------|------|
| G3 `Âᵀ·r̂` + `t̂·r̂` → `[û \| tr̂]` | [`../at_r5/`](../at_r5/) 单核 `f203_encrypt_at_r5`（kP=5，host 拼 `matM`） |
| SIM launch 编排 | 单 ACL session（`run_g5_sim_full` / `main_encrypt_g5_run.cpp`） |
| 原理 | [`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md) |

## Agent 规则

- **可进入**阅读关闭说明与历史审计（[`G3_SIM_AUDIT.md`](../../G3_SIM_AUDIT.md) §1–§11 原文 + §12 修正）
- **禁止**把 frozen 内源码 `#include`、复制、移植到活跃 `compute/at_r5/` 或 `main_encrypt*`
- **禁止**把 frozen 内核加回 `CMakeLists.txt` `KERNEL_FILES`
- Gate 过渡路线（G0–G4）见 [`../../frozen-gates/FROZEN.md`](../../frozen-gates/FROZEN.md)
