# STATUS — RB-T06-cipher-pack

| 字段 | 值 |
|------|-----|
| 刀 | T06 · D-EXP-T06 / G5 |
| 状态 | **PASS**（CPU + `SIM_DIRECT=1` sim） |
| 日期 | 2026-09-08 |
| 墙钟 | ~8 min（编码+双跑） |

## 目标达成

1. `KERNEL_TYPE_AIV_ONLY`，`blockDim=1`；无 MIX / CrossCore
2. I/O：`u[1024] int32` + `v[256] int32` → `c[1568]` uint8
3. 布局：`c₁` = BE₁₁(Compress₁₁(u)) @ `[0,1408)`；`c₂` = BE₅(Compress₅(v)) @ `[1408,1568)`（见 `LAYOUT.md`）
4. Host golden 与设备/CPU 孪生逐字节一致（max=0）

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；c[1568] match |
| `SIM_DIRECT=1 bash run.sh -r sim …` | SUCCESS；totalTick≈57699；无 stray dump |
| sync_audit | **无红线**（仅 SYNC-09 性能；无 CrossCore） |

运营回报：[`../../encrypt-rebuild-ops/tasks/T06-cipher-pack-c1c2/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T06-cipher-pack-c1c2/FEEDBACK.md)
