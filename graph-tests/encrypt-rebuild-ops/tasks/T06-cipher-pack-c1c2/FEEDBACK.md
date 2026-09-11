# FEEDBACK — T06-cipher-pack-c1c2

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS
cmd: cd graph-tests/bricks/RB-T06-cipher-pack && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: 8
sync_audit: 无红线（仅 SYNC-09 性能；无 CrossCore）
sim: ok
notes:
- 新建 RB-T06：AIV-only；u/v → Compress11/5 + BE → c[1568]；c1@[0,1408) c2@[1408,1568)。
- LAYOUT.md 写清偏移；CPU/SIM 逐字节对拍 golden。
- 未抄 encrypt/alg14；未改 KB/DAG；未并行第二路 SIM；未碰 NPU/SSH。
next_hint: 主控关闭 G5；下一刀由 QUEUE 定。
```

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/cpu.log`](logs/cpu.log) | CPU 全量 |
| [`logs/sim.log`](logs/sim.log) | SIM_DIRECT 全量 |
| [`logs/sync_audit.json`](logs/sync_audit.json) | sync_audit（无红线） |

## 实现 STATUS

[`../../../bricks/RB-T06-cipher-pack/STATUS.md`](../../../bricks/RB-T06-cipher-pack/STATUS.md)

## 主控批注

（待主控）

## 主控批注

- 主控 NPU：**PASS**（T06_NPU_EXIT:0；c=1568B）
