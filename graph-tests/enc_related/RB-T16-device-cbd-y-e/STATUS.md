# STATUS — RB-T16-device-cbd-y-e

| 字段 | 值 |
|------|-----|
| 刀 | T16 · 设备 CBD coins→(y,e₁,e₂) |
| 状态 | **PASS_CPU**（SIM skip；待主控 NPU） |
| 日期 | 2026-09-08 |
| 墙钟 | ~1 min（独占 CPU） |

## 目标达成

1. Host 预喂 coins（对齐 T07 FIXED_COINS）；**禁**预喂最终 y/e1/e2
2. 设备 MIX：flag **仅 1/3**；AIV0 SHAKE256 PRF(N=0..8) + Alg.8 CBD η=2 → 9 poly
3. CPU PASS_SYNC + PASS_IO（max_abs=0）；sync_audit 无红线（SYNC-05 假阳性同前）
4. 未抄 encrypt/alg14/frozen；未改 KB/DAG；未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO |
| SIM | **skip**（NPU 优先） |
| sync_audit | 无红线；SYNC-05 + SYNC-09 |

运营回报：[`../../encrypt-rebuild-ops/tasks/T16-device-cbd-y-e/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T16-device-cbd-y-e/FEEDBACK.md)
