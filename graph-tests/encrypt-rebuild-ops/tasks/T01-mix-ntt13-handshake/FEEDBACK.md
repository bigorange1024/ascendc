# FEEDBACK — T01-mix-ntt13-handshake

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS
cmd: cd graph-tests/toys/RB-T01-mix-ntt13-handshake && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: 7
sync_audit: clean（无红线；9× SYNC-09 PIPE_ALL 性能）
sim: ok
notes:
- 新建 RB-T01：MIX 1AIC+2AIV；flag 1/3；极轻 Cube 16×32×32；Host 单 launch。
- CPU TRACE 全槽直写绿；SIM 上 AIC/AIV1 标量 TRACE 常空，靠 causal(SET1+WAIT3⇒WAIT1+SET3)+mat_c 非零验收。
- sync_audit JSON 已入 logs/；exit 0。
- 未改 KB/DAG；未抄 encrypt/alg14；未并行第二路 SIM。
next_hint: NPU 轨可复跑本玩具（N01）；SIM 下一刀 T04 或 T02 由主控定。
```

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit 全量 |
| [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU TRACE / SUCCESS 摘录 |
| [`logs/sim-excerpt.txt`](logs/sim-excerpt.txt) | SIM TRACE / causal / SUCCESS 摘录 |

## 实现 STATUS

[`../../../toys/RB-T01-mix-ntt13-handshake/STATUS.md`](../../../toys/RB-T01-mix-ntt13-handshake/STATUS.md)

## 主控批注

- 2026-09-08 回收：**PASS**（CPU+SIM exit 0；sync_audit 无红线）。  
- 沉淀：SIM 上 AIC/AIV1 标量 TRACE 常空 → 用 causal + `mat_c` 非零（KB 增补）。  
- 解锁 T02 / T07；下一 SIM 刀派 **T04**；NPU 在 N00 冒烟后派 **N01** 复跑本玩具。
