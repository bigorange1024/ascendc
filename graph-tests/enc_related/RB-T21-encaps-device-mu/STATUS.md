# STATUS — RB-T21-encaps-device-mu

| 字段 | 值 |
|------|-----|
| 刀 | T21 · D-EXP-T21 |
| 状态 | **PASS_CPU**（PASS_SYNC + PASS_IO）；SIM **skip**（战役 NPU 优先） |
| 日期 | 2026-09-08 |
| 墙钟 | CPU 全链路（编+跑）≈22s；kernel≈4.86s |

## 目标达成

1. 相对 T20：μ 改由**设备** Decompress₁(m)（T04）；Host 只预装原始 m→OFF_M，**不**预喂最终 μ
2. 其余 Encrypt 全链同 T20；K 仍 Host G 对拍
3. CPU：c/K/μ_dev/u/v/t̂/Â/ŷ/yee/ρ 对拍 max=0；TRACE 含 MU_DONE；sync_audit 无红线（SYNC-05 假阳性同前刀）
4. 硬锁 `F203_AHAT16_BLOCK_DIM=1`；Flag 1/3+4；永禁 5/7
5. 未抄 encaps/alg14/alg20/encrypt/frozen；未改 KB/DAG；Subagent 未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO（kernel≈4.86s） |
| `SIM_DIRECT=1 … sim` | **skip**（NPU 优先；非本战役门禁） |
| sync_audit | 无红线；SYNC-05 + SYNC-09 |
| 用例根 stray dump | 无（本轮未跑 SIM） |

## 关键锁

- `F203_AHAT16_BLOCK_DIM=1` / `F203_CBD_BLOCK_DIM=1`
- CrossCore flag **1/3 复用 + 4=GATE**；永禁 **5/7**
- Host 禁预喂最终 μ/t̂/y/e/Â/ŷ/u/v/c
- out magic `0x543F0015`（T21）

运营回报：[`../../encrypt-rebuild-ops/tasks/T21-encaps-device-mu/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T21-encaps-device-mu/FEEDBACK.md)
