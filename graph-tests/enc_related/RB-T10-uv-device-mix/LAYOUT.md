# LAYOUT — RB-T10 u,v 设备 MIX 真拓扑

## 数据流（一句话）

Host 预喂 NTT 域 Â/ŷ/t̂ 与时域 e₁/e₂/μ（及 ζ/γ）→ **设备** MultiplyNTTs 得 û/v̂ → GATE → **设备** INTT+加噪得 u,v。

## 与 T08 差异

T08 = Host 孪生算 u,v；T10 = 同输入契约，**u,v 必须在 MIX AIV0 算出**（禁 Host 预喂最终 u,v）。

## Flag（T03 纪律）

| flagId | 含义 |
|--------|------|
| **1** | AIV→AIC 就绪（NTT/INTT 复用） |
| **3** | AIC→AIV Cube 完成（复用） |
| **4** | GATE |
| 禁 | **5 / 7** |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 POST_WAIT3_NTT 无 MUL_DONE | 卡 MultiplyNTTs |
| 有 SET4 无 INTT | 卡 GATE |
| 有 PRE_SET1_INTT 无 POST_WAIT3_INTT | 卡 INTT 复用 |
| 有 POST_WAIT3_INTT 无 UV_DONE | 卡 INTT+加噪 |
