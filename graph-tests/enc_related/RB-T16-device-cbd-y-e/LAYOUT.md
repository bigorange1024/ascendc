# LAYOUT — RB-T16 设备 coins→(y,e₁,e₂)

## 数据流（一句话）

Host coins[32]（T07 语义）→ **设备** SHAKE256 PRF(N=0..8) → Alg.8 CBD η=2 → y‖e1‖e2。

## 与 T07 / alg8 / lines8–15 关系

| 刀/探针 | 角色 |
|--------|------|
| T07 | Host prep：coins→(y,e₁,e₂)；本刀复用其 **coins 字节与 nonce 表** |
| alg8 CBD | Phase C 积木 `SamplePolyCbd2OneRowUb`（CMake `-I`，不抄源） |
| shake_xof | Phase P：`ShakeXofUb` SHAKE256（Encrypt：coins 当 σ；**无** KeyGen Phase G） |
| T13 | MIX 半链壳（flag 1/3 + 极轻 Cube）；本刀同构换载荷 |

## Flag

| flagId | 含义 |
|--------|------|
| **1** | AIV→AIC 就绪 |
| **3** | AIC→AIV Cube 完成 |
| 禁 | **5 / 7**（本刀无 GATE） |

## 输出布局

- `y_e1_e2[9,256] int32` 平面：行 0–3=y，4–7=e₁，8=e₂
- 另拆 `y.bin` / `e1.bin` / `e2.bin` 对齐 T07

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡 CBD 握手 |
| 有 POST_WAIT3 无 CBD_DONE | 卡 PRF/CBD / 写出 |
| YEE 有但 max≠0 | coins / SHAKE256 / CBD 契约差 |
