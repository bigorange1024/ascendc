# FEEDBACK — N02-npu-encrypt-skel

> 主控直跑（NPU 轨；与 T09 并行上板，不等 SIM）。

```
ID: PASS_SYNC_NPU + PASS_IO_NPU
cmd: cd graph-tests/enc_related/RB-T09-encrypt-shaped-2launch && ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3
exit: 0
wall_min: <1（kernel≈2.5s；含首次 NPU 编）
sync_audit: 沿用 T09 本机（无红线）；NPU 以 SynchronizeStream+causal+产物为准
sim: n/a（本刀 NPU）
notes:
- 代码目录同 T09；主控 flock + 清产物后上板。
- Launch1 prep + Launch2 compute：sync 返回；TRACE 因果 NTT→GATE→INTT + pack；c[1568]/y/ρ IO 绿。
- 日志：../T09-encrypt-shaped-2launch/logs/npu-parallel-053311.log
next_hint: Q-ULT 挂死维可收口本外形；真 Âᵀ∘ŷ 设备拓扑另开刀。
```

## 日志索引

| 文件 | 说明 |
|------|------|
| [`../T09-encrypt-shaped-2launch/logs/npu-parallel-053311.log`](../T09-encrypt-shaped-2launch/logs/npu-parallel-053311.log) | NPU 全量 |

## 主控批注

- 2026-09-08：主控并行上板，**PASS**（不挂 + IO）。
