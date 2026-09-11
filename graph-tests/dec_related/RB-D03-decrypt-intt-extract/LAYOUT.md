# LAYOUT — RB-D03 Decrypt L2b INTT+extract→m

## 数据流（一句话）

Host 预喂 `ŵ`/`v`/`ζ` → **设备 MIX** Alg.10 INTT(ŵ)→w；ByteEncode₁(Compress₁(v−w))→m[32]。

## 与 D01 / D02 关系

| 刀 | 角色 |
|----|------|
| D01 | prep：写出 `v[256]` int32（本刀输入契约） |
| D02 | L2a：写出 `ŵ[256]` int32（本刀输入契约；无 pad） |
| D03 | L2b：INTT+extract → m |

## Flag

| flagId | 含义 |
|--------|------|
| **4** | GATE（AIV→AIC 收口） |
| **1** | AIV→AIC 就绪 |
| **3** | AIC→AIV Cube 完成 |
| 禁 | **5 / 7** |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡 GATE/INTT 握手 |
| 有 POST_WAIT3 无 DONE | 卡 InverseNTT / extract / 写出 |
| m 有但 max≠0 | ζ 表、INTT_SCALE、Compress₁ 或 v−w ModQ |
