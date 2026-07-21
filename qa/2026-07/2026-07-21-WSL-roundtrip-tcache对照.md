# 2026-07-21 — WSL `stable_kem_liboqs_roundtrip` tcache 对照矩阵

任务：排查连续 SIM 时 Decaps Phase-D 后 `tcache_thread_shutdown(): unaligned tcache chunk detected`（见 [2026-07-20 纪要](2026-07-20-Decaps交付stable与registry.md) §WSL tcache）。

**约束**：未改 stable 算法；无 branch/commit/push。原始日志：`/tmp/tcache_rt/`。

## 环境

| 项 | 值 |
|----|-----|
| host | WSL2 |
| Mem | **3.7 Gi** total；全程 available ≈ **2.6–2.8 Gi**；free 常落到 **0.5 Gi**（buff/cache 高） |
| Swap | 1.0 Gi，未观察到显著 swap 使用 |

## 对照结果（本日 **均未复现** tcache）

| 组 | 条件 | EXIT | fixture | 结果 |
|----|------|------|---------|------|
| EXP1 | 全链 CPU+SIM（`set -o pipefail` + tee） | **0** | `…/20260721_012751_1955` | cpu+sim **PASS** |
| EXP2 | `CAMODEL_SKIP_ADX_WORK_PATH=1` + `SKIP_CPU=1` | **0** | `…/20260721_021528_7098` | sim **PASS** |
| EXP1b | 基线再跑 `SKIP_CPU=1`（连跑加压） | **0** | `…/20260721_030429_10970` | sim **PASS** |
| EXP3 | Phase 间 sleep **45s** + `SKIP_CPU=1` | **0** | `…/20260721_035228_14442` | sim **PASS** |
| EXP4a | 只 KeyGen→Decaps（c=fixture） | **0** | `…/split_kg_20260721_044327_18008` | **PASS** |
| EXP4b | 只 Encaps→Decaps（ek/dk=fixture） | **0** | `…/split_enc_20260721_050752_21066` | **PASS** |

- 全部 `/tmp/tcache_rt/*.log`：**无** `tcache` / `Aborted`
- 无 core → **未做** gdb（失败当次取证条件未触发）

## 假说（更新）

| 假说 | 本日证据 |
|------|----------|
| **算法/接线 bug** | **仍弱**：历史 CPU 绿 + 单 Decaps SIM 绿；本日 6 组连续 SIM 亦全绿 |
| **dump/ADX（WSL stub vs Cloud skip）** | **未区分**：有/无 `CAMODEL_SKIP_ADX_WORK_PATH=1` 均绿；**不能**据此排除（未炸则无法证明消因） |
| **内存压力 / CAModel 连续 teardown** | **仍最像**：历史失败形态在 Phase-D 后 glibc 线程退出；本机仅 3.7 Gi；但本日压力下仍未炸 → **偶发**，需更大样本或更紧内存才能钉死 |
| **sleep 缓解** | 未验证（基线已绿；sleep 组也绿） |

**结论（有条件）**：倾向维持「**环境偶发（CAModel/acl teardown 堆损坏检出）**，非 stable 正确性回归」；本日对照矩阵**未能复现**，故**不能**判定哪组能「消掉」炸点。建议：办公室再遇炸时立刻 `dmesg` + core + gdb；可选加压（更小 WSL 内存 / 连跑 ≥10 次全链 SIM）。

## 原始产物

- 汇总：`/tmp/tcache_rt/report.txt`
- 日志：`exp1_baseline.log` / `exp2_adx.log` / `exp1b_baseline.log` / `exp3_sleep.log` / `exp4a_kg_dec.log` / `exp4b_enc_dec.log`
