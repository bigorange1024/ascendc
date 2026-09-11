# STATUS — RB-T10-uv-device-mix

| 字段 | 值 |
|------|-----|
| 刀 | T10 · D-EXP-T10 / G3 device |
| 状态 | **PASS_SYNC + PASS_IO**（CPU + SIM_DIRECT） |
| 日期 | 2026-09-08 |
| 墙钟 | ~25 min（编码）+ CPU~17s + SIM kernel~356s |

## 目标达成

1. Host 预喂 Â/ŷ/t̂/e₁/e₂/μ（及 ζ/γ）；**禁**预喂最终 u,v
2. 设备 MIX：T03 flag **1/3 复用 + GATE=4**；AIV0 **MultiplyNTTs / 内积 → INTT+加噪** 写出 u,v
3. CPU/SIM u,v max_abs=0；sync_audit 无红线（SYNC-05 薄封装假阳性同 T03）
4. 未抄 alg14/encrypt/frozen；未改 KB/DAG；未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO |
| `SIM_DIRECT=1 bash run.sh -r sim …` | PASS；totalTick≈778232；wall≈356s；无 hang |
| sync_audit | 无红线；SYNC-05(高假阳性)+SYNC-09 性能 |
| 用例根 stray dump | 无（已收拢 `sim_log/`） |

运营回报：[`../../encrypt-rebuild-ops/tasks/T10-uv-device-mix/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T10-uv-device-mix/FEEDBACK.md)
