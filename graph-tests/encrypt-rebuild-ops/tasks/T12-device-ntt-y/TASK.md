# T12 — 设备 ŷ←NTT(y)（接 prep）

| 字段 | 值 |
|------|-----|
| 状态 | **ready**（T07 prep / T03 flag 纪律已绿） |
| DAG | `D-EXP-T12`（主控自定下一刀） |
| 代码目录 | `graph-tests/enc_related/RB-T12-device-ntt-y/` |
| 运营目录 | `…/tasks/T12-device-ntt-y/` |
| 墙钟 | ≤ 70 min |

继承 COMMON。

## 目标

Host 预喂或自算 **y[4×256]**（CBD/coins，对齐 T07 prep 语义；**禁抄** encrypt/alg14）→ 设备 **MIX** 做：

\[
\hat y \leftarrow \mathrm{NTT}(y)
\]

- **poly-batch**：每个 AIV 握完整 poly（hi+lo）；禁 limbsplit
- S1–S3 路径若走三段式则 **禁 Gather**；本刀允许 AIV 标量 Alg.9（与 T10 INTT 同形态）但对拍 ŷ
- Flag：**1/3** 握手；**永禁 5/7**（本刀无 GATE 亦可）
- 主验收：**ŷ 对拍** + 不挂

## 非目标

- Â←SampleNTT(ρ)、设备 CBD、u/v 拓扑、pack→c、Encaps

## 必读

- COMMON · KB §A2 · T03/T07/T10 FEEDBACK（壳与 CBD）
- cannbot CrossCore + sync-audit
- 活跃 NTT 探针 **契约**（勿大段抄码）；禁 frozen

## 验收

- CPU + `SIM_DIRECT=1` sim；用例根无 stray
- sync_audit；中文注释；FEEDBACK PASS/FAIL
- 禁并行第二路 SIM；禁 SSH/NPU/改 KB/DAG

## 依赖

T07 prep 语义可复用；flag 纪律对齐 T03。
