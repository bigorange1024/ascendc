# FEEDBACK-E16

| 字段 | 值 |
|------|----|
| task_id | E16 |
| verdict | **PASS** |
| wall_clock_min | **~22**（含首轮 C2 verify 失败 + 全量 smoke 复跑；deadline 40） |
| directory | `graph_tests/npu_suite/` |
| hypothesis | `D-exp-e16` |

## 结果摘要

1. 新建 **NPU 数字 TRACE 套件**（仅 `graph_tests/npu_suite/**`）：薄包装 C0/C1/C2 → 原 toy `run.sh`，未改 E01–E15 源目录。
2. **文档齐**：`SUITE.md`、`RUNBOOK.md`、`TRACE_MASTER.md`、`REPORT_TEMPLATE.md` + `run_all_npu.sh` / `run_smoke_sim.sh`。
3. **三档挂因阶梯**：C0=e01 握手壳；C1=e13 形态粘合；C2=e15 Â2×2 + 粘合。
4. **SIM smoke ×1/档**（`SIM_DIRECT=1`，`TOY_ROUNDS=1`，C1/C2 `TOY_SKIP_GOLDEN=1`）：
   - C0：**PASS**（kernel rc=0，Host 100/101/110/111 ×1）
   - C1：**PASS**（kernel rc=0，TRACE-only verify）
   - C2：**PASS（包装 smoke）** — kernel rc=0 + Host 111；verify 因 **SIM tee 偶发缺 305** 非零（已知，见 toy-e15 TRACE.md）；NPU 取证不以 SIM 305 为准
5. **未做**：ByteDecode、liboqs/KAT、golden 门禁、改 Encrypt/原 toy、并行 SIM、commit/push。

## 交付路径

| 项 | 路径 |
|----|------|
| 套件总览 | `graph_tests/npu_suite/SUITE.md` |
| 上机 RUNBOOK | `graph_tests/npu_suite/RUNBOOK.md` |
| TRACE 总表 | `graph_tests/npu_suite/TRACE_MASTER.md` |
| 回报模板 | `graph_tests/npu_suite/REPORT_TEMPLATE.md` |
| 实机串行入口 | `graph_tests/npu_suite/run_all_npu.sh` |
| C0 包装 | `graph_tests/npu_suite/cases/C0-e01-handshake/run.sh` |
| C1 包装 | `graph_tests/npu_suite/cases/C1-e13-glue/run.sh` |
| C2 包装 | `graph_tests/npu_suite/cases/C2-e15-a2x2/run.sh` |

## smoke 日志

| 档 | 路径 | kernel | 备注 |
|----|------|--------|------|
| C0 | `/opt/cursor/artifacts/e16-c0-sim.log` | rc=0 ~14s | E01 verify OK |
| C1 | `/opt/cursor/artifacts/e16-c1-sim.log` | rc=0 ~86s | TRACE-only |
| C2 | `/opt/cursor/artifacts/e16-c2-sim.log` | rc=0 ~185s | 305 missing in tee |
| C2 retry | `/opt/cursor/artifacts/e16-c2-sim-retry.log` | rc=0 ~177s | 同上 flake |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e16` | **support** | 套件 + 三档 + TRACE 总表 + smoke 包装可跑 |
| `D-trace-digits` | **support** | TRACE_MASTER 对齐知识库 §6；回报模板只填三位号 |
| `D-defer-correctness-until-hang` | **honored** | 无 ByteDecode/KAT/golden 门禁 |
| `J-sim-not-sticky` | cite | SIM smoke 绿 ≠ NPU 不挂；套件供实机取证 |
| `D-no-copy-encrypt` | **honored** | 仅包装 toys |
| `D-no-repeat-retracted` | **honored** | 未复测 retracted |
| `D-short-experiments` | **honored** | 墙钟 ~22min ≤40 |

## 上机提示（主控/用户）

```bash
cd graph_tests/npu_suite
bash run_all_npu.sh -v Ascend910B4   # C0→C1→C2 串行；各 1 轮；跳过 golden
# 回报见 REPORT_TEMPLATE.md + TRACE_MASTER.md
```

## 范围合规

- 白名单仅 `graph_tests/npu_suite/**` + 本 FEEDBACK。
- 未改图谱 yaml；未 commit/push。
