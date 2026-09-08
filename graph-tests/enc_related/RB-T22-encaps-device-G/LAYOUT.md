# LAYOUT — RB-T22 Encaps + 设备 G(m‖H(ek)) + 设备 μ → (c, K_dev)

## 数据流（一句话）

Host：仅预装 m 与 ek；
Launch1 prep：设备 h←H(ek)、(K‖r)←G(m‖h)、coins=r、ρ←ek 尾 → mid-sync →
Launch2：μ←Decompress₁(m)→Decode₁₂→CBD(coins)→Â/ŷ→Mul/INTT→pack→c。

## 与 T21 差异

| 刀 | 差异 |
|----|------|
| T21 | Host coins=r←G、K 转发；设备 μ |
| **T22** | **设备** (K‖r)←G(m‖H(ek))；Host **不**预喂 coins/K；其余同 T21 |

## Flag（Launch2）

1/3 复用；4=GATE；**永禁 5/7**。`BLOCK_DIM=1`。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `m.bin` / `ek.bin` | Encaps 语义输入；m 装入 OFF_M |
| `zetas` / `gammas` / `mat_*` | Launch2 LUT / 极轻 Cube |
| **无** `coins` / `K` / `h` / `mu` / `t_hat` / `y_e1_e2` / `a_hat` / `y_hat` / `u` / `v` / `c` | 禁止预喂 |

## 设备 / Host 输出（`output/`）

`c.bin`、`K_dev.bin`、`h_dev.bin`、`coins_dev.bin`、`mu_dev.bin`、`u/v/t_hat/a_hat/y_hat/yee/ρ/trace`、`golden_*`。

## 硬锁

`F203_AHAT16_BLOCK_DIM=1`（文件头 `#undef`+`#define` + npu_lib `ascendc_compile_definitions`）。
out magic `0x543F0016`（T22）。
