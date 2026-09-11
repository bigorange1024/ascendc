# LAYOUT — RB-T15 设备 Encrypt 半链：Â+ŷ → u,v → c

## 数据流（一句话）

Launch1 prep 落盘 ρ/y‖e1‖e2 → Host mid-sync → Launch2：设备 Â←SampleNTT(ρ)、ŷ←NTT(y)
→ Mul/INTT+噪 → u,v → pack→c[1568]。

## 与 T11 / T14 / T10 / T06 差异

| 刀 | 差异 |
|----|------|
| T11 | 双 launch + 设备 u,v+pack，但 **Host 预喂** Â/ŷ |
| T14 | 单 launch 设备 Â/ŷ，**无** Mul/pack |
| T10 | 单 launch 设备 u,v，Host 预喂 Â/ŷ |
| T06 | 仅 pack；本刀复用契约 |
| **T15** | 双 launch + **设备** Â/ŷ + u,v + pack |

## Flag（Launch2）

1/3 复用；4=GATE；**永禁 5/7**。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `ek.bin` / `y_e1_e2.bin` | Launch1 prep |
| `t_hat`/`e1`/`e2`/`mu`/`zetas`/`gammas` | Launch2 拓扑噪声预喂 |
| `mat_a`/`mat_b` | 极轻 Cube |
| **无** `a_hat`/`y_hat`/`u`/`v` | 禁止预喂最终 Â/ŷ/u/v |

## 设备输出（`output/`）

`c.bin`、`u.bin`、`v.bin`、`a_hat.bin`、`y_hat.bin`、`trace.bin`、`mat_c_*`、`y_e1_e2_dev`、`rho_dev`、`golden_*`。
