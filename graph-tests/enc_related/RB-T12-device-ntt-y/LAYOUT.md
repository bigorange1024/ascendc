# LAYOUT — RB-T12 设备 ŷ←NTT(y)

## 数据流（一句话）

Host CBD(coins)→y[4×256] + ζ → **设备** Alg.9 正向 NTT → ŷ。

## 与 T07 / T10 关系

| 刀 | 角色 |
|----|------|
| T07 | Host prep：ρ/y/e₁/e₂；本刀复用其 **y 采样语义** |
| T10 | 设备 INTT 标量形态；本刀对称做 **正向 NTT** |
| T12 | 关 Encrypt 行 16 设备侧 |

## Flag

| flagId | 含义 |
|--------|------|
| **1** | AIV→AIC 就绪 |
| **3** | AIC→AIV Cube 完成 |
| 禁 | **5 / 7**（本刀无 GATE） |

## poly-batch

AIV0 按 poly 循环；每次握完整 256 系数；禁 limbsplit；禁 Gather。

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 POST_WAIT3_NTT 无 YHAT_DONE | 卡 ForwardNTT / 写出 |
| YHAT 有但 max≠0 | ζ 表 / ModQ / Alg.9 实现差 |
