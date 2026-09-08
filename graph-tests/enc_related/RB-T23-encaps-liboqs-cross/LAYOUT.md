# LAYOUT — RB-T23 Encaps × liboqs 交叉（权威 c/K）

## 数据流（一句话）

Host：liboqs KeyGen→合法 ek + 固定 m；
Launch1 prep：设备 h←H(ek)、(K‖r)←G(m‖h)、coins=r、ρ←ek 尾 → mid-sync →
Launch2：μ←Decompress₁(m)→Decode₁₂→CBD(coins)→Â/ŷ→Mul/INTT→pack→c；
验收：设备 c/K ↔ liboqs Encaps（同 m/ek）。

## 与 T22 差异

| 刀 | 差异 |
|----|------|
| T22 | 合成 ek；golden 自洽 Host Encrypt |
| **T23** | **liboqs KeyGen ek**；**golden_c/K=liboqs Encaps**；缺库 **BLOCKED** |

## Flag（Launch2）

1/3 复用；4=GATE；**永禁 5/7**。`BLOCK_DIM=1`。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `m.bin` / `ek.bin` | m 固定；ek←liboqs KeyGen |
| `zetas` / `gammas` / `mat_*` | Launch2 LUT / 极轻 Cube |
| **无** `coins` / `K` / `h` / `mu` / … | 禁止预喂 |

## 设备 / Host 输出（`output/`）

`c.bin`、`K_dev.bin`、中间量、`golden_c`/`golden_K`（liboqs）、`cross_backend.txt`。

## 硬锁

`F203_AHAT16_BLOCK_DIM=1`；out magic `0x543F0017`（T23）。
