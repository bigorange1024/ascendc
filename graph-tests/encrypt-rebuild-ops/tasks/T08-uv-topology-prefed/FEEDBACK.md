# FEEDBACK — T08-uv-topology-prefed

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS
cmd: cd graph-tests/enc_related/RB-T08-uv-topology && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: 12
sync_audit: N/A（无 AscendC / CrossCore）
sim: skip（无 MIX 核；run.sh 显式 SKIP）
notes:
- 新建 RB-T08：Host 预喂 Â/ŷ/t̂/e₁/e₂/μ → 孪生 u=INTT(Âᵀ∘ŷ)+e₁、v=INTT(⟨t̂,ŷ⟩)+e₂+μ；CPU max=0。
- 拓扑数据流：NTT 域矩阵转置乘/内积 → 双路 INTT → 时域加 e/μ。
- ζ/γ 只读 ntt_onnx 表；μ 用 T04 Decompress₁；禁抄 alg14/encrypt/frozen。
- 无设备核 → 无 flag；日后 MIX 挂点假设：INTT 复用 T03 的 1/3，GATE=4，永禁 5/7。
- 未改 KB/DAG；未并行第二路 SIM；未碰 NPU/SSH。
next_hint: 主控可关 G3 Host 证据；设备化 u,v 另开 MIX 刀（接 T03 handshake）。
```

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/cpu.log`](logs/cpu.log) | CPU Host 对拍全量 |
| [`logs/sim.log`](logs/sim.log) | SIM SKIP 说明 |

## 实现 STATUS

[`../../../enc_related/RB-T08-uv-topology/STATUS.md`](../../../enc_related/RB-T08-uv-topology/STATUS.md)

## 主控批注

- 回收 [T08 uv拓扑](f845fc06-0243-4af9-9003-16787400485f)：**Host CPU PASS**；无 MIX → SIM/NPU SKIP（不进双绿表，关 G3 Host 证据）。
- 已派 [T09 2launch](197d73c7-5967-4ca1-99b8-245d3a11dfe6) 本机；主控 NPU 不等 SIM。
- T02/T03 连跑出现 **X9**（TRACE 落 AIV1、verify 硬绑 AIV0 假红）；已派 [TRACE AIV1修复](858c3201-4795-4a02-84aa-a90029f210a0)，修完立刻 rsync 重上板。
