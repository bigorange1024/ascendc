# FEEDBACK — T12-device-ntt-y

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_SYNC + PASS_IO
cmd: cd graph-tests/enc_related/RB-T12-device-ntt-y && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: ~7（编码+双跑+审计；SIM kernel≈134s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T02/T03/T10) + SYNC-09 性能
sim: ok（totalTick≈282419；wall≈134s；无 hang）
notes:
- 新建 RB-T12：单 launch MIX；Host CBD(coins)→y + ζ；AIV0 设备 Alg.9 正向 NTT 写 ŷ。
- Flag：仅 1/3；永禁 5/7；本刀无 GATE；AIC 极轻 Cube×1（握手有界真算）。
- poly-batch：逐 poly 握完整 256 系数；禁 limbsplit；禁 Gather；未走 S1–S3 Cube NTT。
- CPU/SIM 均 PASS_SYNC+PASS_IO（ŷ max=0）；SIM AIC/AIV1 TRACE soft 常空。
- 未改 KB/DAG；未抄 encrypt/alg14/frozen；未并行第二路 SIM；未碰 NPU/SSH。
next_hint: 主控可回收行 16 设备侧；后刀可接 SampleNTT(ρ) 或与 T10/T11 拼装。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（NTT 1/3 因果 + Cube 非零 + sync 无红线） |
| **PASS_IO** | 是（ŷ[4,256] 与 golden 一致） |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 POST_WAIT3_NTT 无 YHAT_DONE | 卡 ForwardNTT / 写出 |
| YHAT 有但 max≠0 | ζ / ModQ / Alg.9 差 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU |
| [`logs/sim.log`](logs/sim.log) / [`logs/sim-excerpt.txt`](logs/sim-excerpt.txt) | SIM_DIRECT |

## 实现 STATUS

[`../../../enc_related/RB-T12-device-ntt-y/STATUS.md`](../../../enc_related/RB-T12-device-ntt-y/STATUS.md)

## 主控批注

- **SIM**：回收 [T12设备NTT(y)半链](1854cb28-f5b8-471d-83a8-569c3c9af144) — CPU+SIM **PASS_SYNC+PASS_IO**（ŷ max=0）；SYNC-05 假阳性同前。
- **NPU**（2026-09-08）：**T12_EXIT:0** PASS_SYNC+PASS_IO（ŷ）；sticky T09 同役绿 → **双绿入账**。续跑 T10/T11 循环占卡中。
