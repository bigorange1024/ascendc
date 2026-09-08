# T08 — u,v 拓扑：预喂中间量（G3）

| 字段 | 值 |
|------|-----|
| 状态 | **blocked_dep**（T03 + T04 + T05） |
| DAG | `D-EXP-T08` → G3 |
| 代码目录 | `graph-tests/enc_related/RB-T08-uv-topology/` |
| 运营目录 | `…/tasks/T08-uv-topology-prefed/` |
| 墙钟 | ≤ 55 min |

继承 COMMON。

## 目标

Host **预生成** Â、ŷ、t̂、e₁、e₂、μ（μ 来自 T04 逻辑或 bin）→ 设备或 CPU 孪生完成：

\[
u=\mathrm{INTT}(\hat A^T\circ\hat y)+e_1,\quad
v=\mathrm{INTT}(\langle\hat t,\hat y\rangle)+e_2+\mu
\]

对拍 u,v。同步若用 MIX：遵守 T03 flag 纪律；**禁抄** alg14 compute。

## 必读

- inventory G3 · innerproduct / INTT notes · KB §A2  
- T03/T04/T05 FEEDBACK

## 验收

- u,v vs golden；CPU+SIM（若 MIX）  
- sync_audit（若有 CrossCore）  
- FEEDBACK：拓扑数据流一句话 + 挂点假设

## 依赖

T03 + T04 + T05 = PASS。
