# LAYOUT — RB-D05 Decaps Re-Encrypt：ek + m' + r' → c'[1568]

## 数据流

```text
Launch1 reenc_prep_custom（AIV-only）
  r'/coins → OFF_COINS；ek → OFF_EK；ρ←ek 尾
Host mid-sync
Launch2 reenc_mix_custom（MIX，flag 1/3+4，BLOCK_DIM=1）
  μ←Decompress₁(m') → Decode₁₂(ek)→t̂ → CBD → Â/ŷ → Mul/INTT → pack → c'[1568]
```

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `ek.bin` | 1568B PKE ek |
| `m_prime.bin` | 32B；装入 OFF_M（非最终 μ） |
| `r_prime.bin` | 32B coins；喂 prep |
| `zetas.bin` / `gammas.bin` / `mat_a.bin` / `mat_b.bin` | LUT / 极轻 Cube |
| **无** 最终 `c'` / `μ` / `t̂` / `y` / `u` / `v` | 禁止预喂 |

## 设备输出（`output/`）

`c_prime.bin`（权威对拍）、诊断 `mu_dev`/`u`/`v`/`t_hat_dev`/`a_hat`/`y_hat`/`y_e1_e2_dev`/`rho_dev`/`trace`/`mat_c_*`、`golden_*`、`cross_backend.txt`。
