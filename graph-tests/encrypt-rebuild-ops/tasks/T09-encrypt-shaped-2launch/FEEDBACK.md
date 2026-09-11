# FEEDBACK — T09-encrypt-shaped-2launch

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_SYNC + PASS_IO
cmd: cd graph-tests/enc_related/RB-T09-encrypt-shaped-2launch && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: 25
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T02/T03) + SYNC-09 性能
sim: ok（totalTick≈129256；wall≈62s；无 hang）
notes:
- 新建 RB-T09：单库双核 Launch1 prep(AIV)+Launch2 compute(MIX)；禁双设备库。
- prep：T07 语义半桩（Host CBD→设备落盘 y/ρ）；compute：T03 flag 1/3+GATE4 + Cube×2 + Host 预喂 u,v pack→c。
- CPU/SIM 均 PASS_SYNC + PASS_IO（c/y/ρ 对齐）；SIM AIC/AIV1 TRACE soft 常空。
- 未改 KB/DAG；未抄 encrypt/alg14/frozen；未并行第二路 SIM；未碰 NPU/SSH。
- NPU 复现草稿：logs/npu-repro-draft.md
next_hint: 主控 N02 可按 npu-repro-draft 上板；设备真拓扑另开刀。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（双 launch 不挂 + TRACE 因果 + Cube 非零 + sync 无红线） |
| **PASS_IO** | 是（c[1568]、y_e1_e2_dev、rho_dev 与 golden 一致） |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU |
| [`logs/sim.log`](logs/sim.log) / [`logs/sim-excerpt.txt`](logs/sim-excerpt.txt) | SIM_DIRECT |
| [`logs/npu-repro-draft.md`](logs/npu-repro-draft.md) | 供 N02 |

## 实现 STATUS

[`../../../enc_related/RB-T09-encrypt-shaped-2launch/STATUS.md`](../../../enc_related/RB-T09-encrypt-shaped-2launch/STATUS.md)

## 主控批注

- **SIM**：回收 [T09 Encrypt外形2launch](197d73c7-5967-4ca1-99b8-245d3a11dfe6) — CPU+SIM_DIRECT **PASS_SYNC+PASS_IO**；sync_audit 无红线。
- **NPU 并行（不等 SIM）**：rsync 后 `ASCEND_DEVICE_ID=0` / 910B3；`T09_NPU_EXIT:0`（wall≈2.5s）；复跑 `T09b:0`。
- 证据：`logs/npu-parallel-053311.log` — **PASS_SYNC + PASS_IO**。**双绿+N02 同证**。
- 下一刀：G3 **设备 MIX** 真拓扑（替换 Host 预喂 u,v）；任务书 T10。
