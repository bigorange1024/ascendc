# FEEDBACK — T05-bytedecode12-probe

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS
cmd: cd graph-tests/bricks/RB-T05-bytedecode12 && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: 8
sync_audit: N/A（无 CrossCore）
sim: ok
notes:
- 新建 RB-T05：AIV-only；k=4 BE₁₂ 1536B→t̂[1024]；薄壳 include shared byte_decode12_vec.hpp 标量路径。
- Host Python encode/decode round-trip 自洽；CPU/SIM max=0。
- 未抄 alg14/encrypt；未改 KB/DAG；未并行第二路 SIM。
next_hint: 主控关闭 G-BD12；下一积木刀由 QUEUE 定。
```

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/cpu.log`](logs/cpu.log) | CPU 全量 |
| [`logs/sim.log`](logs/sim.log) | SIM_DIRECT 全量 |

## 实现 STATUS

[`../../../bricks/RB-T05-bytedecode12/STATUS.md`](../../../bricks/RB-T05-bytedecode12/STATUS.md)

## 主控批注

（待主控）
- 2026-09-08 主控 NPU 上板：**PASS**（RB-T05）；主控连续占卡中。
