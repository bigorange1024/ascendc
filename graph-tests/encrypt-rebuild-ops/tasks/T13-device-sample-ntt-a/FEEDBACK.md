# FEEDBACK — T13-device-sample-ntt-a

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_SYNC + PASS_IO
cmd: cd graph-tests/enc_related/RB-T13-device-sample-ntt-a && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: ~16（编码+双跑+审计；含 1 次 SIM 半边失败重跑；末次 SIM kernel≈355s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T02/T03/T10/T12) + SYNC-09 性能
sim: ok（totalTick≈705084；wall≈355s；无 hang）
notes:
- 新建 RB-T13：单 launch MIX；Host ρ[32]（T07 FIXED_RHO）→ AIV0 设备 16×Alg.7 SampleNTT 写 Â。
- Flag：仅 1/3；永禁 5/7；本刀无 GATE；AIC 极轻 Cube×1。
- 积木：CMake -I 接 alg7/lines3-7/`BuildAHat16ShardWithUb`；禁大段抄码；禁 encrypt/frozen。
- 锁定 F203_AHAT16_BLOCK_DIM=1（MIX 单 AIV0 跑满 16 poly）；首轮 SIM 曾因默认=2 只写半边。
- CPU/SIM 均 PASS_SYNC+PASS_IO（Â max=0）；SIM AIC/AIV1 TRACE soft 常空。
- 未改 KB/DAG；未并行第二路 SIM；未碰 NPU/SSH。
next_hint: 主控可回收行 3–7 设备侧；后刀可与 T10/T11/T12 拼装 Encrypt 矩阵半链。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（SampleNTT 1/3 因果 + Cube 非零 + sync 无红线） |
| **PASS_IO** | 是（Â[16,256] 与 golden 一致） |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡 SampleNTT 握手 |
| 有 POST_WAIT3 无 AHAT_DONE | 卡 SampleNTT / 写出 |
| AHAT 有但 max≠0 / 半边 0 | BLOCK_DIM≠1 或 ρ/XOF 契约差 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU |
| [`logs/sim.log`](logs/sim.log) / [`logs/sim-excerpt.txt`](logs/sim-excerpt.txt) | SIM_DIRECT |

## 实现 STATUS

[`../../../enc_related/RB-T13-device-sample-ntt-a/STATUS.md`](../../../enc_related/RB-T13-device-sample-ntt-a/STATUS.md)

## 主控批注

- **SIM**：回收 [T13设备SampleNTT(ρ)](6e916403-8bb5-4dfc-b010-138faf28f99b) — CPU+SIM **PASS_SYNC+PASS_IO**（Â max=0）；`BLOCK_DIM=1` 锁定要点已记。
- **NPU**：早试旧树 FAIL_IO；修版推远端后 **T13_EXIT:0**（ROUND13）→ **双绿入账**。
