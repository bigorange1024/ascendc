# LAYOUT — RB-T09 Encrypt 外形双 launch I/O

## Host 输入（`input/`）

| 文件 | 形状 | 说明 |
|------|------|------|
| `ek.bin` | 1568 B | ML-KEM-1024 `ek_PKE`；Launch1 取尾 ρ |
| `coins.bin` | 32 B | 仅 gen_data 记录；设备核不读 |
| `y_e1_e2.bin` | `[9,256] int32` | T07 Host 预算；Launch1 拷入 ws |
| `u.bin` / `v.bin` | `[4,256]` / `[256] int32` | T08 半桩预喂；Launch2 pack |
| `mat_a.bin` / `mat_b.bin` | 16×32 / 32×32 int8 | 极轻 Cube |

## 设备输出（`output/`）

| 文件 | 说明 |
|------|------|
| `out.bin` | Launch2 成功魔数 |
| `c.bin` | pack 密文 1568 B |
| `trace.bin` | PREP + NTT/GATE/INTT/PACK TRACE |
| `mat_c_ntt.bin` / `mat_c_intt.bin` | 两段有界 Cube |
| `y_e1_e2_dev.bin` / `rho_dev.bin` | Launch1 落盘 |
| `golden_*` | Host oracle |

## Flag（Launch2）

1/3 复用；4=GATE；**永禁 5/7**。
