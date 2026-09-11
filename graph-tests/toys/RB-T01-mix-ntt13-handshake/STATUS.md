# STATUS — RB-T01-mix-ntt13-handshake

| 字段 | 值 |
|------|-----|
| 刀 | T01 · D-EXP-T01 |
| 状态 | **PASS**（CPU + `SIM_DIRECT=1` sim） |
| 日期 | 2026-09-08 |
| 墙钟 | ~7 min（编码+双跑） |

## 目标达成

1. `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`
2. CrossCore：`AIV SET(1) → AIC WAIT(1) → 极轻 Cube(16×32×32) → AIC SET(3) → AIV WAIT(3)`；mode `0x2`；禁 flag 5/7 / SoftSync / Wait 环内 SyncAll
3. Host 单 launch + `SynchronizeStream`；`trace_map.md` + Host TRACE 打印（含 causal WAIT1/SET3）
4. CPU / SIM 均 exit 0，无 hang

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；TRACE 全槽直写绿 |
| `SIM_DIRECT=1 bash run.sh -r sim …` | SUCCESS；AIC/AIV1 标量 TRACE 槽常空（WARN）；causal+`mat_c` 非零证 Cube |
| `sync_audit --check all` | exit 0；**无红线**（仅 SYNC-09 PIPE_ALL 性能） |
| 用例根 stray `core*.dump` / `profile_*` | 无 |

运营回报：[`../../encrypt-rebuild-ops/tasks/T01-mix-ntt13-handshake/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T01-mix-ntt13-handshake/FEEDBACK.md)

## 已知 SIM 差异

CAModel 上 AIC（及 AIV1）对 TRACE GM 标量写常不落盘；CPU 孪生全槽可见。验收用 AIV0 SET1/WAIT3 + Host sync + `mat_c` + causal 日志覆盖 A4。
