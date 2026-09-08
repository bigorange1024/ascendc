# FEEDBACK — T03-ntt-intt-handshake-bounded

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS
cmd: cd graph-tests/toys/RB-T03-ntt-gate-intt-bounded && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: 8
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 误解析假阳性，同 T02) + SYNC-09 性能
sim: ok
notes:
- 新建 RB-T03：单 launch MIX NTT(1/3)→GATE(4)→INTT(1/3 复用)；两段极轻 Cube。
- Flag：1/3 复用、4=GATE；永禁 5/7；trace_map 含若挂卡点假设。
- CPU TRACE 全绿；SIM AIC/AIV1 标量 TRACE 常空，靠 AIV0 因果 + 双 mat_c 非零。
- 未改 KB/DAG；未抄 encrypt；未并行第二路 SIM；未碰 NPU/SSH。
- 若挂假设：无 POST_WAIT3_NTT→卡 NTT；有 SET4 无 INTT→卡 GATE；有 PRE_SET1_INTT 无 POST_WAIT3_INTT→卡 INTT 复用。
next_hint: 本会话立刻做 T07 prep 壳（Host CPU）。
```

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit 源码扫描 |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU 全量 / 摘录 |
| [`logs/sim.log`](logs/sim.log) / [`logs/sim-excerpt.txt`](logs/sim-excerpt.txt) | SIM 全量 / 摘录 |

## 实现 STATUS

[`../../../toys/RB-T03-ntt-gate-intt-bounded/STATUS.md`](../../../toys/RB-T03-ntt-gate-intt-bounded/STATUS.md)

## 主控批注

- **SIM**：回收 [催T03勿停](6ba4e998-63af-4256-b73c-f73ab8a5c6bb) — CPU+SIM_DIRECT PASS，sync_audit 无红线。
- **NPU**：rsync 后 `ASCEND_DEVICE_ID=0` / 910B3 / 清产物；`T03_NPU_EXIT:0`；causal NTT→GATE→INTT + dual Cube ok（`logs/npu-051624.log`）。**双绿入账**。
- 下一刀：T08 拓扑（已派本机）；T07 仅 Host，不进 NPU 双绿表。

## 补丁（2026-09-08）— TRACE AIV0|AIV1 成对验收

```
ID: PATCH-TRACE-PAIR
cmd: cd graph-tests/toys/RB-T03-ntt-gate-intt-bounded && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
notes:
- 背景：NPU TRACE 可能落 AIV1 而非 AIV0（KB X7/X9）；硬绑 AIV0 假红。
- 改动：verify 对 NTT/GATE/INTT 五类 AIV 事件改为成对槽 OR；补槽 16 AIV1_POST_WAIT3_NTT；Host+out+双 mat_c 仍硬。
- 本机 CPU+SIM_DIRECT 双绿；未改 KB/DAG；未抄 encrypt；未碰 NPU/SSH。
```

## 主控批注（X9 上板）

- 回收 [TRACE AIV1修复](858c3201-4795-4a02-84aa-a90029f210a0)；rsync 全量（含槽16）清 build 重编。
- NPU：`T03_X9_EXIT:0`（`logs/npu-x9-053007.log`）；成对槽 WARN 可接受；**双绿维持**。
