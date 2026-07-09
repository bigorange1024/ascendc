# 2026-07-09 — Encrypt 探针默认 `SIM_DIRECT` + 全链 I/O 对齐回顾

## 1. 用户约束：默认即最优路径，勿再要求手动编译/运行选项

用户明确：测试阶段可用编译/运行选项切代码段；**代码稳定后**须改成默认跑最优最正确路径，**不要让用户再手动输入更多选项**（含 `SIM_DIRECT=1`）。

说明：`SIM_DIRECT` 本身不是编译宏，而是 sim 是否走 msprof/`OPPROF_*` 的运行开关。成熟探针（keygen/encaps）早已在 `run.sh` 的 sim 分支内 `export SIM_DIRECT=1`；Encrypt 系列文档却仍写 `SIM_DIRECT=1 bash run.sh …`，违反「默认 = 生产全量」口径。

## 2. 已改（2026-07-09）

下列 PASS 探针 `run.sh` 在 `RUN_MODE=sim` 时自动 `export SIM_DIRECT="${SIM_DIRECT:-1}"`；Usage 注释改为：

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4   # 无需手动 SIM_DIRECT
```

| 探针 | 改动 |
|------|------|
| `pass-fix-f203-alg14-pke-encrypt-device-k4` | run.sh + STATUS + INTEGRATION_PLAN |
| `pass-fix-f203-alg14-lines3-15-encrypt-prep-k4` | run.sh |
| `pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` | run.sh |
| `pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4` | run.sh |
| `pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4` | run.sh |

`AGENT_HANDOFF.md` smoke 命令同步去掉手动 `SIM_DIRECT=1`。调试采性能仍可显式 `SIM_DIRECT=0`（非默认）。

## 3. 全链 Encrypt 现状（承接 07-08）

- 探针：`pass-fix-f203-alg14-pke-encrypt-device-k4`
- I/O：in `ek+m+coins` → **out 仅密文 c**（u/v 不落盘）
- 验收：CPU+SIM `c` max=0；SIM ~626k tick；`SEED_D=20260619`
- 家里续测：`git pull` 后直接 `bash run.sh -r cpu|sim -v Ascend910B4`
