# LAYOUT — RB-K03 KeyGen L2b Â∘ŝ̂+ê̂ + ByteEncode₁₂

## 数据流（一句话）

Host 预喂 Â/ŝ̂/ê̂/γ/ρ → **设备 MIX** 点积+BE → `ek_pke[1568]`、`dk_pke[1536]`。

## 与 K01 / K02 关系

| 刀 | 角色 |
|----|------|
| K01 | prep：seed→Â/ŝ/ê/ρ；本刀 Â/ρ 契约对齐 |
| K02 | L2a：NTT(ŝ)+NTT(ê)；本刀输入 ŝ̂/ê̂ |
| K03 | L2b：Â∘ŝ̂+ê̂ → BE₁₂ → ek‖ρ / dk |

## Flag

| flagId | 含义 |
|--------|------|
| **4** | GATE（AIV→AIC 收口） |
| **1** | AIV→AIC 就绪 |
| **3** | AIC→AIV Cube 完成 |
| 禁 | **5 / 7** |

## 数学

\[
\hat{t}[p]=\mathrm{mod}_q\Bigl(\sum_{j=0}^{3}\mathrm{MultiplyNTTs}(\widehat{A}[p,j],\hat{s}[j])+\hat{e}[p]\Bigr)
\]

Â 行主序：`flat(p,j,c)=(p·4+j)·256+c`。

## I/O

| 文件 | 形状 | 角色 |
|------|------|------|
| `input/a_hat.bin` | `[16,256] int32` | Â（SampleNTT，NTT 域） |
| `input/s_ntt.bin` | `[4,256] int32` | NTT(ŝ) |
| `input/e_ntt.bin` | `[4,256] int32` | NTT(ê) |
| `input/gammas.bin` | `[128] int32` | Alg.11 γ 表 |
| `input/rho.bin` | `32B` | ρ |
| `output/ek_pke.bin` | `1568B` | BE₁₂(t̂)‖ρ |
| `output/dk_pke.bin` | `1536B` | BE₁₂(ŝ̂) |
| `output/t_hat.bin` | `[4,256] int32` | 中间 t̂（辅验） |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡 GATE/握手 |
| 有 POST_WAIT3 无 DONE | 卡点积 / BE / 写出 |
| ek/dk 有但 mism | γ 表、Â 布局、或 BE pack 差 |
