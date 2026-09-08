# T10 — u,v 设备 MIX 真拓扑（G3 device）

| 字段 | 值 |
|------|-----|
| 状态 | **ready**（T03/T08/T09 PASS） |
| DAG | `D-EXP-T10` → 关 G-TOPO 设备侧 |
| 代码目录 | `graph-tests/enc_related/RB-T10-uv-device-mix/` |
| 运营目录 | `…/tasks/T10-uv-device-mix/` |
| 墙钟 | ≤ 70 min |

继承 COMMON。

## 目标

在 **设备 MIX** 上完成（禁 Host 预喂最终 u,v 当唯一路径）：

\[
u=\mathrm{INTT}(\hat A^T\circ\hat y)+e_1,\quad
v=\mathrm{INTT}(\langle\hat t,\hat y\rangle)+e_2+\mu
\]

- Host 可预生成 Â、ŷ、t̂、e₁、e₂、μ（同 T08 输入契约）并 H2D
- **计算**须在设备：MultiplyNTTs / 内积编排 + INTT（可复用积木契约，禁抄 alg14/encrypt）
- Flag 纪律同 T03：1/3 复用、4=GATE；永禁 5/7
- 可单 launch 或接在 T09 外形后；主验收 **u,v 对拍** + 不挂

## 非目标

- 完整 SampleNTT(ρ) / CBD 设备化（可用 Host 预喂）
- Compress/pack→c（已有 T06/T09）
- Encaps 外壳

## 必读

- T03/T08/T09 FEEDBACK · inventory G3 · KB §A2 · §X1/X9
- cannbot CrossCore + sync-audit

## 验收

- CPU + `SIM_DIRECT=1` sim；用例根无 stray
- sync_audit 无红线（假阳性注明）
- FEEDBACK：PASS/FAIL + 挂点假设；注明与 T08 Host 孪生差异
- 主控会 **并行 NPU**（你禁 SSH）；本机做完即停

## 依赖

T03 + T08 + T09 = PASS。
