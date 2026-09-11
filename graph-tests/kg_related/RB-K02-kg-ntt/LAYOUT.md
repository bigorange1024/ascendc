# LAYOUT — RB-K02 KeyGen L2a NTT(ŝ)+NTT(ê)

## 数据流（一句话）

Host 预喂时域 `ŝ`/`ê`/`ζ` → **设备 MIX** Alg.9 → NTT(ŝ)、NTT(ê)。

## 与 K01 / K03 关系

| 刀 | 角色 |
|----|------|
| K01 | prep：seed→Â/ŝ/ê；本刀输入契约对齐其 `ŝ`/`ê` 尺寸与 SEED_D |
| K02 | L2a：NTT(ŝ)+NTT(ê) |
| K03 | L2b：Â∘ŝ+ê → ByteEncode → ek/dk（下一刀） |

## Flag

| flagId | 含义 |
|--------|------|
| **4** | GATE（AIV→AIC 收口） |
| **1** | AIV→AIC 就绪 |
| **3** | AIC→AIV Cube 完成 |
| 禁 | **5 / 7** |

## poly-batch

AIV0 按 poly 循环；每次握完整 256 系数；禁 limbsplit；禁 Gather。

## I/O

| 文件 | 形状 | 角色 |
|------|------|------|
| `input/s.bin` | `[4,256] int32` | 时域 ŝ（Host） |
| `input/e.bin` | `[4,256] int32` | 时域 ê |
| `input/zetas.bin` | `[128] int32` | Alg.9 ζ 表 |
| `output/s_ntt.bin` | `[4,256] int32` | 设备 NTT(ŝ) |
| `output/e_ntt.bin` | `[4,256] int32` | 设备 NTT(ê) |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡 GATE/NTT 握手 |
| 有 POST_WAIT3 无 DONE | 卡 ForwardNTT / 写出 |
| s_ntt/e_ntt 有但 max≠0 | ζ 表或 ModQ / Alg.9 实现差 |
