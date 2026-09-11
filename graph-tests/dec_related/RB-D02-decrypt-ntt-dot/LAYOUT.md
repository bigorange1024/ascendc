# LAYOUT — RB-D02 Decrypt L2a NTT(u)+su_dot

## 数据流（一句话）

Host 预喂 `u`/`ŝ`/`ζ`/`γ` → **设备 MIX** Alg.9 NTT(u)→û；Σ Alg.11 MultiplyNTTs(ŝ,û)→ŵ。

## 与 D01 / D03 关系

| 刀 | 角色 |
|----|------|
| D01 | prep：dk_pke+c → ŝ/u/v；本刀输入契约对齐其 `u`/`ŝ` 尺寸 |
| D02 | L2a：NTT+su_dot → û/ŵ |
| D03 | L2b：INTT+extract → m（下一刀） |

## Flag

| flagId | 含义 |
|--------|------|
| **4** | GATE（AIV→AIC 收口） |
| **1** | AIV→AIC 就绪 |
| **3** | AIC→AIV Cube 完成 |
| 禁 | **5 / 7** |

## poly-batch

AIV0 按 poly 循环；每次握完整 256 系数；禁 limbsplit；禁 Gather。

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡 GATE/NTT 握手 |
| 有 POST_WAIT3 无 DONE | 卡 ForwardNTT / MultiplyNTTs / 写出 |
| û/ŵ 有但 max≠0 | ζ/γ 表或 ModQ / Alg.9·11 实现差 |
