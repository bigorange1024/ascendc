# FEEDBACK — T11-encrypt-shaped-device-uv

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_SYNC + PASS_IO
cmd: cd graph-tests/enc_related/RB-T11-encrypt-shaped-device-uv && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: ~50（编码+CPU~17s+SIM kernel≈424s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T02/T03/T09/T10) + SYNC-09 性能
sim: ok（totalTick≈884917；wall≈424s；无 hang）
notes:
- 新建 RB-T11：单库双核 Launch1 prep(AIV)+Launch2 compute(MIX)；禁双设备库。
- prep：T07/T09 语义半桩；compute：T03 flag 1/3+GATE4 + Cube×2 + T10 设备 Mul/INTT→u,v + T06 pack→c。
- 禁 Host 预喂最终 u,v；Host 仅预喂 Â/ŷ/t̂/e/μ/ζ/γ。
- CPU/SIM 均 PASS_SYNC+PASS_IO（c/u/v/prep 对齐）；SIM AIC/AIV1 TRACE soft 常空。
- MAT 置于 prep 后低偏移（远偏移时 CPU Fixpipe 曾静默空写）。
- 未改 KB/DAG；未抄 encrypt/alg14/frozen；未并行第二路 SIM；未碰 NPU/SSH。
next_hint: 主控可关外形拼装本机证据；NPU 波由主控独占上板。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（双 launch 不挂 + PREP/NTT/GATE/INTT/PACK TRACE + Cube 非零 + sync 无红线） |
| **PASS_IO** | 是（c[1568]、u/v、y_e1_e2_dev、rho_dev 与 golden 一致） |

## 与 T09 / T10 差异（一句）

T09=外形但 Host 预喂 u,v；T10=单 launch 设备 u,v；**T11=双 launch + 设备 u,v + pack**。

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 PREP_DONE | 卡 Launch1 |
| 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 POST_WAIT3 无 MUL/SET4 | 卡 MultiplyNTTs |
| 有 SET4 无 INTT | 卡 GATE |
| 有 UV 无 PACK | 卡 pack |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU |
| [`logs/sim.log`](logs/sim.log) / [`logs/sim-excerpt.txt`](logs/sim-excerpt.txt) | SIM_DIRECT |

## 实现 STATUS

[`../../../enc_related/RB-T11-encrypt-shaped-device-uv/STATUS.md`](../../../enc_related/RB-T11-encrypt-shaped-device-uv/STATUS.md)

## 主控批注

- **SIM**：回收 [T11 外形接设备uv](a5d14a31-f93b-4963-b688-9d8c83d33a5f) — CPU+SIM **PASS_SYNC+PASS_IO**。
- **NPU**（2026-09-08）：同役 **T11_EXIT:0** — PASS_SYNC+PASS_IO（c/u/v/prep）；**双绿入账**。
