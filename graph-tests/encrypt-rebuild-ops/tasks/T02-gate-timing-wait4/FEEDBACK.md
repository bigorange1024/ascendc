# FEEDBACK — T02-gate-timing-wait4

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS
cmd: cd graph-tests/toys/RB-T02-gate-timing-wait4 && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: 12
sync_audit: 无红线；1× SYNC-05(高: Wait 后 CrossSet 误解析为 LightCube::DataCopy 假阳性) + SYNC-09 性能
sim: ok
notes:
- 新建 RB-T02：MIX GATE Wait(4)+flag1/3；AIC 首条 CrossCore=Wait(4)；Cube 在 Wait(1)→Set(3)。
- AIC 在 AIV Set4 之前进入 Wait4（预期，靠 Set 唤醒）；Host 打 gate/causal 行。
- CPU TRACE 全绿；SIM AIC/AIV1 标量 TRACE 常空，靠 SET4+WAIT3 causal + mat_c 非零。
- 未改 KB/DAG；未抄 encrypt；未并行第二路 SIM；未碰 NPU/SSH。
next_hint: 解锁 T03；本会话继续 T06 cipher-pack。
```

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit 源码扫描 |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU 全量 / 摘录 |
| [`logs/sim.log`](logs/sim.log) / [`logs/sim-excerpt.txt`](logs/sim-excerpt.txt) | SIM 全量 / 摘录 |

## 实现 STATUS

[`../../../toys/RB-T02-gate-timing-wait4/STATUS.md`](../../../toys/RB-T02-gate-timing-wait4/STATUS.md)

## 主控批注

（待主控）

## 主控批注

- 2026-09-08：SIM PASS 后主控已上板（见 logs/npu-*.log）。

## 主控批注

- 主控 NPU：**PASS**（T02_NPU_EXIT:0）

## 补丁（2026-09-08）— TRACE AIV0|AIV1 成对验收

```
ID: PATCH-TRACE-PAIR
cmd: cd graph-tests/toys/RB-T02-gate-timing-wait4 && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
notes:
- 背景：NPU 上 TRACE 槽可能随机落在 AIV1 而非 AIV0（KB X7/X9）；硬绑 AIV0 会假红。
- 改动：verify_result.py 对 PRE_SET4 / POST_WAIT3 改为成对槽任一魔数匹配；Host+out+mat_c 仍硬；空侧 soft WARN。
- 本机 CPU+SIM_DIRECT 双绿；未改 KB/DAG；未抄 encrypt；未碰 NPU/SSH。
```

## 主控批注（X9 上板复验）

- rsync 补丁后 NPU：`T02_X9_EXIT:0`（成对槽 WARN 可接受）；T06 保活绿。
- T08 Host 已关 G3；清单 G1–G5 Host/砖块状态已刷新。
