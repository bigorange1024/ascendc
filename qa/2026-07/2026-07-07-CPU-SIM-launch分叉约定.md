# 2026-07-07 — Encrypt prep/compute 晋级 pass- 与 CPU/SIM 分叉定案

## 背景

1. **compute 收尾**：kP=5 统一内积、INTT k=8 pad、行 21 `v←INTT(tr̂)+e₂`、行 2 decode 均在 SIM 单 launch 验收通过。
2. **pass 判定分歧**：用户确认单 launch **不支持 CPU**（tikicpu MIX 死锁）为平台约束；探针 pass 须**分平台表述**，不能笼统写「双模式完成」。
3. **目录晋级**：两探针自 `fix-` 重命名为 `pass-`，迁入 `ascendc-tests/INDEX.md` **当前探针**表。

## 定案：目录重命名

| 旧名 | 新名 | 晋级理由 |
|------|------|----------|
| `fix-f203-alg14-lines3-15-encrypt-prep-k4` | `pass-fix-f203-alg14-lines3-15-encrypt-prep-k4` | CPU+SIM 双模式 a_hat/re max=0 |
| `fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` | `pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` | SIM 单 launch 行 2/18/19/21 全量；CPU 部分对照 |

全仓引用已同步：`INDEX.md`、`INTEGRATION_PLAN`、`STATUS`、`AGENT_HANDOFF`、`docs/notes/`、`.gitignore`。

## 定案：compute pass 分平台表述

| 平台 | 判定 | 验收面 |
|------|------|--------|
| SIM 默认单 launch | **完成** | y_hat, u_ntt, u_tr, u, v |
| CPU 三 launch | **部分对照** | y_hat, u_ntt, u（`run.sh` 跳过 u_tr/v） |
| 全探针严格口径 | **有条件完成** | 生产 = SIM；CPU 为分叉指南 §2 例外 |

**根因**：`f203_encrypt_l18_l19` 长 MIX FSM；tikicpu 串行 → AIC Wait 时 AIV 未 Set → 死锁。

## 定案：CPU/SIM host 分叉（上午）

| 项 | 内容 |
|---|---|
| **主分叉轴** | 编译期 `ASCENDC_CPU_DEBUG`（`RUN_MODE=cpu` vs `sim`） |
| **CPU** | `RunCpuThreeLaunch` 仅此一条；无 getenv |
| **SIM 默认** | `RunSimFusedSingleLaunch` |
| **SIM 调试** | `ASCENDC_SIM_HOST_MODE=phased_launch` → `RunSimThreeLaunch` |
| **废弃** | `F203_FEAS_FUSED`、`F203_FEAS_PHASED`、`UseFusedLaunch()` |

全仓统一宏：[`library/shared/ascendc_build_mode.hpp`](../../library/shared/ascendc_build_mode.hpp)  
定稿指南：[docs/notes/AscendC-CPU与SIM实现分叉开发指南.md](../../docs/notes/AscendC-CPU与SIM实现分叉开发指南.md)

## 验收证据（2026-07-07）

```text
prep:   CPU + SIM_DIRECT  a_hat/re max=0
compute: CPU              y_hat/u_ntt/u max=0（u_tr/v 跳过）
compute: SIM_DIRECT        y_hat/u_ntt/u_tr/u/v max=0；tick ~125k
```

## 代码注释

核心入口已补详细中文文件头：

- compute：`f203_encrypt_l18_l19_kernel.cpp`、`f203_encrypt_at_jp_vec.hpp`、`main.cpp`
- prep：`f203_encrypt_prep_entry.cpp`、`main.cpp`

## 文档刷新

- `AGENT_HANDOFF.md` — 2026-07-07 全量覆盖
- `ascendc-tests/INDEX.md` — 两探针迁入当前探针表
- 各 `STATUS.md` / `INTEGRATION_PLAN.md` §8

## 下一任务（P0）

1. prep + compute **GM 级拼接** → 目标 Encrypt 核心 2 launch
2. Alg.21 Decaps 单 session SIM 真修
3. correctness `run.sh` 资源友好化
