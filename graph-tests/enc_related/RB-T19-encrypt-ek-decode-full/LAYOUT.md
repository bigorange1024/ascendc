# LAYOUT — RB-T19 全设备 Encrypt：Decode₁₂(ek)→t̂ + CBD+Â/ŷ→u,v→c

## 数据流（一句话）

Launch1 prep：coins→ws、完整 ek→OFF_EK、ρ←ek 尾 → Host mid-sync → Launch2：
Decode₁₂(ek)→t̂ → CBD(coins)→y/e → Â←SampleNTT(ρ)、ŷ←NTT(y) → Mul/INTT+噪 → u,v → pack→c[1568]。

## 与 T17 / T18 差异

| 刀 | 差异 |
|----|------|
| T17 | 双 launch + 设备 CBD+Â/ŷ/u,v/pack，但 **Host 预喂** t̂ |
| T18 | 单 launch 仅设备 Decode₁₂(ek)→t̂ |
| **T19** | 双 launch + **设备 Decode₁₂** + CBD + Â/ŷ + u,v + pack |

## Flag（Launch2）

1/3 复用；4=GATE；**永禁 5/7**。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `ek.bin` / `coins.bin` | Launch1 prep；ek=BE₁₂(t̂)‖ρ |
| `mu` / `zetas` / `gammas` | Launch2 拓扑 LUT/μ |
| `mat_a` / `mat_b` | 极轻 Cube |
| **无** `t_hat` / `y_e1_e2` / `a_hat` / `y_hat` / `u` / `v` / `c` | 禁止预喂最终值 |

## 设备输出（`output/`）

`c.bin`、`u.bin`、`v.bin`、`t_hat_dev.bin`、`a_hat.bin`、`y_hat.bin`、`y_e1_e2_dev.bin`、`rho_dev.bin`、`trace.bin`、`mat_c_*`、`golden_*`。

## 硬锁

`F203_AHAT16_BLOCK_DIM=1`（文件头 `#undef`+`#define` + npu_lib `ascendc_compile_definitions`）。
