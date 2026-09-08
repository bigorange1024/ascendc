# LAYOUT — RB-T07 Encrypt prep 壳 I/O

## 输入

| 文件 | 形状 | 说明 |
|------|------|------|
| `input/ek.bin` | 1568 B | ML-KEM-1024 `ek_PKE` = ByteEncode₁₂(t̂)[1536] ‖ ρ[32] |
| `input/coins.bin` | 32 B | Alg.14 随机币 `r` |

## 输出

| 文件 | 形状 | 说明 |
|------|------|------|
| `output/rho.bin` | 32 B | `ek[-32:]` |
| `output/y.bin` | `[4,256] int32` | CBD_η₂(PRF(coins,0..3)) |
| `output/e1.bin` | `[4,256] int32` | CBD_η₂(PRF(coins,4..7)) |
| `output/e2.bin` | `[256] int32` | CBD_η₂(PRF(coins,8)) |
| `output/y_e1_e2.bin` | `[9,256] int32` 平面 | y‖e1‖e2，供后刀 |

## PRF

`SHAKE256(coins ‖ byte(N), 128)`（FIPS 203 PRF_η，η=2）。  
CBD：`golden_se_sampling.sample_poly_cbd2`（shared；禁抄 encrypt）。
