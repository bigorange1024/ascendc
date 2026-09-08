# FEEDBACK — T14-device-ahat-yhat

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_SYNC + PASS_IO
cmd: cd graph-tests/enc_related/RB-T14-device-ahat-yhat && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: ~27（编码+双跑+审计；末次 SIM kernel≈457s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T02/T03/T10/T12/T13) + SYNC-09 性能
sim: ok（totalTick≈956912；wall≈457s；无 hang）
notes:
- 新建 RB-T14：单 launch MIX；Host ρ+y/ζ → AIV0 同核顺序 SampleNTT(ρ) 再 NTT(y)；双对拍 Â/ŷ。
- Flag：仅 1/3；永禁 5/7；本刀无 GATE；AIC 极轻 Cube×1。
- 积木：CMake -I 接 alg7/lines3-7；锁定 F203_AHAT16_BLOCK_DIM=1 + ascendc_compile_definitions。
- CPU/SIM 均 PASS_SYNC+PASS_IO（Â/ŷ max=0）；SIM AIC/AIV1 TRACE soft 常空。
- 未改 KB/DAG；未并行第二路 SIM（末次验收串行）；未碰 NPU/SSH。
next_hint: 主控可回收行 3–7‖16 设备半链；后刀接 T10 MultiplyNTTs / T11 拼装。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（1/3 因果 + Cube 非零 + AHAT/YHAT TRACE + sync 无红线） |
| **PASS_IO** | 是（Â[16,256] 与 ŷ[4,256] 均与 golden 一致） |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡握手 |
| 有 POST_WAIT3 无 AHAT_DONE | 卡 SampleNTT / BLOCK_DIM≠1 |
| 有 AHAT 无 YHAT_DONE | 卡 ForwardNTT / 写出 |
| Â/ŷ max≠0 | ρ/y/ζ 契约差 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU |
| [`logs/sim.log`](logs/sim.log) / [`logs/sim-excerpt.txt`](logs/sim-excerpt.txt) | SIM_DIRECT |

## 实现 STATUS

[`../../../enc_related/RB-T14-device-ahat-yhat/STATUS.md`](../../../enc_related/RB-T14-device-ahat-yhat/STATUS.md)

## 主控批注

（待）
