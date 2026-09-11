# LAYOUT — RB-T24 Encaps→liboqs Decaps 往返（K≡K'）

## 数据流（一句话）

Host：liboqs KeyGen→(ek,sk) + 固定 m；
设备 Encaps（同 T23）→ (c,K)；
验收：liboqs Decaps(sk,c)→K' ≡ K。

## 与 T23 差异

| 刀 | 差异 |
|----|------|
| T23 | 设备 c/K ↔ liboqs Encaps 交叉 |
| **T24** | **主门：liboqs Decaps(sk, device_c)→K' ≡ K_dev**；Encaps golden 仅诊断 |

## Flag（Launch2）

1/3 复用；4=GATE；**永禁 5/7**。`BLOCK_DIM=1`。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `m.bin` / `ek.bin` | m 固定；ek←liboqs KeyGen |
| `zetas` / `gammas` / `mat_*` | Launch2 LUT / 极轻 Cube |
| **无** `coins` / `K` / `h` / `mu` / … | 禁止预喂 |

## 设备 / Host 输出（`output/`）

`c.bin`、`K_dev.bin`、`sk.bin`（KeyGen dk）、`K_prime_liboqs_decaps.bin`、中间量、`golden_c`/`golden_K`（Encaps 诊断）、`cross_backend.txt`。

## 硬锁

`F203_AHAT16_BLOCK_DIM=1`；out magic `0x543F0018`（T24）。
