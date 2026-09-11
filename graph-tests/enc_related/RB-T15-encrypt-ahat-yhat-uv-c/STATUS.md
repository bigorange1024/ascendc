# STATUS — RB-T15-encrypt-ahat-yhat-uv-c

| 字段 | 值 |
|------|-----|
| 刀 | T15 · D-EXP-T15 |
| 状态 | **PASS_CPU**（PASS_SYNC + PASS_IO）；SIM **skip**（战役 NPU 优先） |
| 日期 | 2026-09-08 |
| 墙钟 | CPU 全链路（编+跑）≈25s；kernel≈6.6s |

## 目标达成

1. Launch1 prep + Launch2：T14 设备 Â/ŷ → T10 Mul/INTT → T06 pack→c
2. Host 预喂 t̂/e/μ；**禁**预喂最终 Â/ŷ/u/v
3. CPU：c/u/v/Â/ŷ 对拍 max=0；sync_audit 无红线（SYNC-05 薄封装假阳性同 T11）
4. 未抄 alg14/encrypt/frozen；未改 KB/DAG；Subagent 未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO（kernel≈6.6s） |
| `SIM_DIRECT=1 … sim` | **skip**（NPU 优先；非本战役门禁） |
| sync_audit | 无红线；SYNC-05 + SYNC-09 |
| 用例根 stray dump | 无（本轮未跑 SIM） |

## 关键锁

- `F203_AHAT16_BLOCK_DIM=1`
- CrossCore flag **1/3 复用 + 4=GATE**；永禁 **5/7**
- 双 launch 单库；禁 Host 预喂最终 Â/ŷ/u/v

运营回报：[`../../encrypt-rebuild-ops/tasks/T15-encrypt-ahat-yhat-uv-c/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T15-encrypt-ahat-yhat-uv-c/FEEDBACK.md)
