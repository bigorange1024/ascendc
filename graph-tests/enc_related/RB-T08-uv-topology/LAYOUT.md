# LAYOUT — RB-T08 u,v 拓扑（预喂中间量）

## 数据流（一句话）

预喂 NTT 域 Â/ŷ/t̂ 与时域 e₁/e₂/μ → Host 孪生算 û=Âᵀ∘ŷ、v̂=⟨t̂,ŷ⟩ → INTT → 加噪得 u,v。

## 输入（Host 预生成）

| 文件 | 形状 | 说明 |
|------|------|------|
| `input/a_hat.bin` | `[4,4,256] int32` | Â，行主序 `(p,j,*)→(p*4+j)*256` |
| `input/y_hat.bin` | `[4,256] int32` | ŷ（NTT 域） |
| `input/t_hat.bin` | `[4,256] int32` | t̂（NTT 域） |
| `input/e1.bin` | `[4,256] int32` | e₁ 时域 |
| `input/e2.bin` | `[256] int32` | e₂ 时域 |
| `input/mu.bin` | `[256] int32` | μ（T04：Decompress₁） |
| `input/m.bin` | `32 B` | 消息（生成 μ 的源） |

## 输出

| 文件 | 形状 | 说明 |
|------|------|------|
| `output/u.bin` | `[4,256] int32` | `INTT(Âᵀ∘ŷ)+e₁` |
| `output/v.bin` | `[256] int32` | `INTT(⟨t̂,ŷ⟩)+e₂+μ` |
| `output/golden_u.bin` / `golden_v.bin` | 同上 | gen_data oracle |

## 挂点假设（日后 MIX）

本刀无设备核。若接 MIX：INTT 段复用 T03 flag **1/3**，GATE=**4**；**永禁 5/7**；禁 Wait 环内 SyncAll。
