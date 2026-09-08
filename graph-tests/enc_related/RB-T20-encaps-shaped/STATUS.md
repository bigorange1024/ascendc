# STATUS — RB-T20-encaps-shaped

| 字段 | 值 |
|------|-----|
| 刀 | T20 · D-EXP-T20 |
| 状态 | **PASS_CPU**（PASS_SYNC + PASS_IO）；SIM **skip**（战役 NPU 优先） |
| 日期 | 2026-09-08 |
| 墙钟 | CPU 全链路（编+跑）≈22s；kernel≈4.65s |

## 目标达成

1. 相对 T19：Encaps 外形 — Host μ←m（T04）；coins=r←G(m‖H(ek))；Host K 对拍
2. 设备 Encrypt 全链不变：Decode₁₂(ek)+CBD+Â/ŷ→u,v→pack→c
3. CPU：c/K/μ/u/v/t̂/Â/ŷ/yee/ρ 对拍 max=0；sync_audit 无红线（SYNC-05 假阳性同 T19）
4. 硬锁 `F203_AHAT16_BLOCK_DIM=1`；Flag 1/3+4；永禁 5/7
5. 未抄 encaps/alg14/alg20/encrypt/frozen；未改 KB/DAG；Subagent 未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO（kernel≈4.65s） |
| `SIM_DIRECT=1 … sim` | **skip**（NPU 优先；非本战役门禁） |
| sync_audit | 无红线；SYNC-05 + SYNC-09 |
| 用例根 stray dump | 无（本轮未跑 SIM） |

## 关键锁

- `F203_AHAT16_BLOCK_DIM=1` / `F203_CBD_BLOCK_DIM=1`
- CrossCore flag **1/3 复用 + 4=GATE**；永禁 **5/7**
- Encaps 外形本刀：Host μ/K；设备侧 H/G→K 后刀
- 禁 Host 预喂最终 t̂/y/e/Â/ŷ/u/v/c

运营回报：[`../../encrypt-rebuild-ops/tasks/T20-encaps-shaped/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T20-encaps-shaped/FEEDBACK.md)
