# STATUS — RB-T12-device-ntt-y

| 字段 | 值 |
|------|-----|
| 刀 | T12 · 设备 ŷ←NTT(y) |
| 状态 | **PASS_SYNC + PASS_IO**（CPU + SIM_DIRECT） |
| 日期 | 2026-09-08 |
| 墙钟 | ~7 min（编码+双跑+审计） |

## 目标达成

1. Host CBD(coins)→y[4×256] + ζ；**禁**预喂最终 ŷ
2. 设备 MIX：flag **仅 1/3**；AIV0 **Alg.9 ForwardNTT** 写出 ŷ（poly-batch 整 poly；禁 Gather）
3. CPU/SIM ŷ max_abs=0；sync_audit 无红线（SYNC-05 薄封装假阳性同 T03/T10）
4. 未抄 alg14/encrypt/frozen；未改 KB/DAG；未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO |
| `SIM_DIRECT=1 bash run.sh -r sim …` | PASS；totalTick≈282419；wall≈134s；无 hang |
| sync_audit | 无红线；SYNC-05(高假阳性)+SYNC-09 性能 |
| 用例根 stray dump | 无（已收拢 `sim_log/`） |

运营回报：[`../../encrypt-rebuild-ops/tasks/T12-device-ntt-y/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T12-device-ntt-y/FEEDBACK.md)
