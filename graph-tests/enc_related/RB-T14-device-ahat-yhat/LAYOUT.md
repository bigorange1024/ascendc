# LAYOUT — RB-T14 设备 Â+ŷ 半链

## 数据流（一句话）

Host ρ[32] + y[4×256]/ζ → **单 launch MIX** → AIV0 同核顺序 Â←SampleNTT(ρ)、ŷ←NTT(y)。

## 与 T12 / T13 关系

| 刀 | 角色 |
|----|------|
| T12 | 仅 ŷ←NTT(y)；本刀复用其 NTT 契约 |
| T13 | 仅 Â←SampleNTT(ρ)；本刀复用其积木 + `BLOCK_DIM=1` |
| T14 | 拼装两路；禁 MultiplyNTTs / pack |

## Flag

| flagId | 含义 |
|--------|------|
| **1** | AIV→AIC 就绪 |
| **3** | AIC→AIV Cube 完成 |
| 禁 | **5 / 7** |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡握手 |
| 有 POST_WAIT3 无 AHAT_DONE | 卡 SampleNTT / BLOCK_DIM |
| 有 AHAT 无 YHAT_DONE | 卡 ForwardNTT |
| Â/ŷ max≠0 | ρ/y/ζ 契约或积木接线差 |
