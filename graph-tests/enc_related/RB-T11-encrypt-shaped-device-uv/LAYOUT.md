# LAYOUT — RB-T11 Encrypt 外形：设备 u,v + pack

## 数据流（一句话）

Launch1 prep 落盘 ρ/y‖e1‖e2 → Host mid-sync → Launch2：Host 预喂 Â/ŷ/t̂/e/μ → **设备** Mul+INTT 得 u,v → pack→c[1568]。

## 与 T09 / T10 差异

| 刀 | 差异 |
|----|------|
| T09 | 外形双 launch，但 **Host 预喂** u,v 再 pack |
| T10 | 单 launch，设备算 u,v，**无** prep / pack |
| **T11** | 双 launch + **设备** u,v + pack（禁 Host 孪生 u,v 唯一路径） |

## Flag（Launch2）

1/3 复用；4=GATE；**永禁 5/7**。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `ek.bin` / `y_e1_e2.bin` | Launch1 prep |
| `a_hat`/`y_hat`/`t_hat`/`e1`/`e2`/`mu`/`zetas`/`gammas` | Launch2 拓扑预喂 |
| `mat_a`/`mat_b` | 极轻 Cube |
| **无** `u.bin`/`v.bin` | 禁止预喂最终 u,v |

## 设备输出（`output/`）

`c.bin`、`u.bin`、`v.bin`、`trace.bin`、`mat_c_*`、`y_e1_e2_dev`、`rho_dev`、`golden_*`。
