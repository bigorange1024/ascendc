# LAYOUT — RB-T17 全设备 Encrypt：CBD+Â/ŷ→u,v→c

## 数据流（一句话）

Launch1 prep：coins→ws、ρ←ek 尾 → Host mid-sync → Launch2：CBD(coins)→y/e
→ Â←SampleNTT(ρ)、ŷ←NTT(y) → Mul/INTT+噪 → u,v → pack→c[1568]。

## 与 T15 / T16 差异

| 刀 | 差异 |
|----|------|
| T15 | 双 launch + 设备 Â/ŷ/u,v/pack，但 **Host 预喂** y/e |
| T16 | 单 launch 仅设备 CBD；**无** Â/Mul/pack |
| **T17** | 双 launch + **设备 CBD** + Â/ŷ + u,v + pack |

## Flag（Launch2）

1/3 复用；4=GATE；**永禁 5/7**。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `ek.bin` / `coins.bin` | Launch1 prep |
| `t_hat` / `mu` / `zetas` / `gammas` | Launch2 拓扑 LUT/μ |
| `mat_a` / `mat_b` | 极轻 Cube |
| **无** `y_e1_e2` / `e1` / `e2` / `a_hat` / `y_hat` / `u` / `v` / `c` | 禁止预喂最终值 |

## 设备输出（`output/`）

`c.bin`、`u.bin`、`v.bin`、`a_hat.bin`、`y_hat.bin`、`y_e1_e2_dev.bin`、`rho_dev.bin`、`trace.bin`、`mat_c_*`、`golden_*`。
