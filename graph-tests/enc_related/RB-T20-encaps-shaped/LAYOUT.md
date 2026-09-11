# LAYOUT — RB-T20 Encaps 外形 + Encrypt 全链 → (c, K_host)

## 数据流（一句话）

Host Encaps 头：μ←m（T04）、(K‖r)←G(m‖H(ek)) → coins=r 喂设备；
Launch1 prep：coins+ek+ρ → Host mid-sync → Launch2：Decode₁₂→CBD→Â/ŷ→Mul/INTT→pack→c[1568]。

## 与 T19 差异

| 刀 | 差异 |
|----|------|
| T19 | 固定 coins + Host 预喂 μ；无 K |
| **T20** | Encaps 外形：m→μ（Host）；coins=r←G；Host K 对拍；设备 Encrypt 同 T19 |

## Flag（Launch2）

1/3 复用；4=GATE；**永禁 5/7**。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `m.bin` / `ek.bin` | Encaps 语义输入 |
| `coins.bin` / `K_host.bin` | gen_data 由 G(m‖H(ek)) 派生；本刀 Host 外形 |
| `zetas` / `gammas` / `mat_*` | Launch2 LUT / 极轻 Cube |
| **无** `t_hat` / `y_e1_e2` / `a_hat` / `y_hat` / `u` / `v` / `c` | 禁止预喂最终值 |

## 设备 / Host 输出（`output/`）

`c.bin`、`K_host.bin`、`mu_host.bin`、`u/v/t_hat/a_hat/y_hat/yee/ρ/trace`、`golden_*`。

## 硬锁

`F203_AHAT16_BLOCK_DIM=1`（文件头 `#undef`+`#define` + npu_lib `ascendc_compile_definitions`）。
