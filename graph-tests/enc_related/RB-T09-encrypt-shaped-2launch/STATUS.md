# STATUS — RB-T09-encrypt-shaped-2launch

| 字段 | 值 |
|------|-----|
| 刀 | T09 · D-EXP-T09 |
| 状态 | **PASS_SYNC + PASS_IO**（CPU + SIM + **NPU**） |
| 日期 | 2026-09-08 |
| 墙钟 | ~25 min（编码+双跑+审计） |

## 目标达成

1. **外形双 launch**（单库）：Launch1 `prep_custom`（AIV-only）→ Host mid-sync → Launch2 `compute_custom`（MIX 1_2）
2. Launch1：承接 T07 语义 — 设备侧落盘 `y‖e1‖e2` + `ρ←ek` 尾（Host 预算 CBD/PRF；半桩）
3. Launch2：T03 级握手 `NTT(1/3)→GATE(4)→INTT(1/3复用)` + 两段极轻 Cube + T08 半桩体（Host 预喂 u,v）+ T06 级 pack→c[1568]
4. Flag：1/3 复用、4=GATE；**永禁 5/7**；见 `trace_map.md`

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；PASS_SYNC + PASS_IO |
| `SIM_DIRECT=1 bash run.sh -r sim …` | SUCCESS；totalTick≈129256；wall≈62s；无 hang |
| `sync_audit --check all` | **无红线**；SYNC-05（高：CrossSet 薄封装误解析假阳性，同 T02/T03）+ SYNC-09 性能 |
| 用例根 stray dump | 无（stray 已收拢 `sim_log/`） |

运营回报：[`../../encrypt-rebuild-ops/tasks/T09-encrypt-shaped-2launch/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T09-encrypt-shaped-2launch/FEEDBACK.md)
