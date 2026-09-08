# LAYOUT — RB-T18 设备 ByteDecode₁₂(ek)→t̂

## 数据流（一句话）

Host ek[1568]=BE₁₂(t̂)‖ρ → **设备** ByteDecode₁₂ → t̂[4×256] int32。

## 与 T05 / prep 关系

| 刀/探针 | 角色 |
|--------|------|
| T05 | AIV-only Decode₁₂ 终态；本刀复用其 **I/O 契约 + shared 头**，不抄源码 |
| T07 | Host prep 外形（ρ←ek 尾）；本刀预喂完整 ek，ρ 仅占位 |
| T16 | MIX 半链壳（flag 1/3 + 极轻 Cube）；本刀同构换载荷 |

## Flag

| flagId | 含义 |
|--------|------|
| **1** | AIV→AIC 就绪 |
| **3** | AIC→AIV Cube 完成 |
| 禁 | **5 / 7**（本刀无 GATE） |

## 输出布局

- `t_hat[4,256] int32` 平面，poly 连续
- `out.bin` 魔数 `0x543A0012`（T18）

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡 Decode 握手 |
| 有 POST_WAIT3 无 BD12_DONE | 卡 Decode / 写出 |
| t̂ 有但 max≠0 | ek 体布局 / Decode₁₂ 契约差 |
