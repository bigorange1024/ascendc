# FEEDBACK — T10-uv-device-mix

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_SYNC + PASS_IO
cmd: cd graph-tests/enc_related/RB-T10-uv-device-mix && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: ~45（编码+双跑；SIM kernel≈356s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T02/T03/T09) + SYNC-09 性能
sim: ok（totalTick≈778232；wall≈356s；无 hang）
notes:
- 新建 RB-T10：单 launch MIX；Host 预喂 Â/ŷ/t̂/e/μ/ζ/γ；AIV0 设备 MultiplyNTTs+INTT 写 u,v。
- Flag：1/3 复用、4=GATE；永禁 5/7；AIC 极轻 Cube×2（握手有界真算）。
- 相对 T08：同输入契约与 golden 公式，但 u,v **必须设备算**（禁 Host 孪生作唯一路径）。
- CPU/SIM 均 PASS_SYNC+PASS_IO（u,v max=0）；SIM AIC/AIV1 TRACE soft 常空。
- 未改 KB/DAG；未抄 encrypt/alg14/frozen；未并行第二路 SIM；未碰 NPU/SSH。
next_hint: 主控可关 G3 设备侧；NPU 波由主控独占上板。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（NTT→MUL→GATE→INTT→UV 因果 + 双 Cube 非零 + sync 无红线） |
| **PASS_IO** | 是（u[4,256]、v[256] 与 golden 一致） |

## 与 T08 差异（一句）

T08 = Host 孪生算 u,v；T10 = 同预喂中间量，**u,v 在 MIX AIV0 真算**。

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 POST_WAIT3_NTT 无 MUL_DONE | 卡 MultiplyNTTs |
| 有 SET4 无 INTT | 卡 GATE |
| 有 PRE_SET1_INTT 无 POST_WAIT3_INTT | 卡 INTT 复用 |
| 有 POST_WAIT3_INTT 无 UV_DONE | 卡 INTT+加噪 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU |
| [`logs/sim.log`](logs/sim.log) / [`logs/sim-excerpt.txt`](logs/sim-excerpt.txt) | SIM_DIRECT |

## 实现 STATUS

[`../../../enc_related/RB-T10-uv-device-mix/STATUS.md`](../../../enc_related/RB-T10-uv-device-mix/STATUS.md)

## 主控批注

- **SIM**：回收 [T10 设备uv](a937a97c-39b6-47c8-b8c7-04affbead050) — CPU+SIM **PASS_SYNC+PASS_IO**（u,v max=0）；G3 设备侧本机证据成立。
- **NPU**（2026-09-08）：补 `ntt_onnx` 表头后战役 **T10_EXIT:0** — `[PASS_SYNC]+[PASS_IO]` u,v；**双绿入账**。 sticky T06/T03/T09 同役绿。
