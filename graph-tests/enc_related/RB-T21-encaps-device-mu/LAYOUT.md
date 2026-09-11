# LAYOUT — RB-T21 Encaps + 设备 μ←Decompress₁(m) → (c, K_host)

## 数据流（一句话）

Host：coins=r←G(m‖H(ek))、K 转发、**仅预装原始 m**；
Launch1 prep → mid-sync → Launch2：μ←Decompress₁(m)→Decode₁₂→CBD→Â/ŷ→Mul/INTT→pack→c。

## 与 T20 差异

| 刀 | 差异 |
|----|------|
| T20 | Host μ←m（T04）；设备 Encrypt |
| **T21** | **设备** μ←Decompress₁(m)；Host **不**预喂最终 μ；其余同 T20 |

## Flag（Launch2）

1/3 复用；4=GATE；**永禁 5/7**。`BLOCK_DIM=1`。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `m.bin` / `ek.bin` | Encaps 语义输入；m 装入 OFF_M |
| `coins.bin` / `K_host.bin` | gen_data 由 G(m‖H(ek)) 派生 |
| `zetas` / `gammas` / `mat_*` | Launch2 LUT / 极轻 Cube |
| **无** `mu` / `t_hat` / `y_e1_e2` / `a_hat` / `y_hat` / `u` / `v` / `c` | 禁止预喂最终值 |

## 设备 / Host 输出（`output/`）

`c.bin`、`K_host.bin`、`mu_dev.bin`、`u/v/t_hat/a_hat/y_hat/yee/ρ/trace`、`golden_*`。

## 硬锁

`F203_AHAT16_BLOCK_DIM=1`（文件头 `#undef`+`#define` + npu_lib `ascendc_compile_definitions`）。
