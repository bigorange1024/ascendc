# STATUS — RB-T03-ntt-gate-intt-bounded

| 字段 | 值 |
|------|-----|
| 刀 | T03 · D-EXP-T03 |
| 状态 | **PASS**（CPU + `SIM_DIRECT=1` sim） |
| 日期 | 2026-09-08 |
| 墙钟 | ~8 min（编码+双跑+审计） |

## 目标达成

1. `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`，单 launch
2. 三段：`NTT(1/3)` → `GATE Wait(4)` → `INTT(复用 1/3)`；两段极轻 Cube → `mat_c_ntt` / `mat_c_intt`
3. Flag 表：1/3 复用、4=GATE；**永禁 5/7**；见 `trace_map.md`
4. CPU / SIM 均 exit 0，无 hang；用例根无 stray dump

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；TRACE 全槽直写绿；phases 因果齐 |
| `SIM_DIRECT=1 bash run.sh -r sim …` | SUCCESS；causal+双 mat_c；totalTick≈6118 |
| `sync_audit --check all`（源码） | **无红线**；SYNC-05（高，CrossSet 薄封装误解析假阳性，同 T02）+ SYNC-09 性能 |
| 用例根 stray | 无 |
| 2026-09-08 补丁 | verify：AIV TRACE 硬条件改为 AIV0\|AIV1 成对槽任一魔数；补槽 16 AIV1_POST_WAIT3_NTT |

运营回报：[`../../encrypt-rebuild-ops/tasks/T03-ntt-intt-handshake-bounded/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T03-ntt-intt-handshake-bounded/FEEDBACK.md)
