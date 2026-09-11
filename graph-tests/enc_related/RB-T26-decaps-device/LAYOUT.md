# LAYOUT — RB-T26 Alg.21 Decaps 设备多 launch

## 数据流（一句话）

Host 预装 `dk_kem`+`c`+ζ/γ/mat →
**L1–L3 Decrypt**（T25 契约）：ŝ/u/v → û/ŵ → m' → mid-sync →
**L4–L5 Reenc**（T22/T23 契约）：(K'‖r)←G(m'‖H(ek)) → Encrypt→c' →
Host：`c'≡c ? K' : J(z‖c)` → `K[32]`。

## Flag

| Launch | Flag | 说明 |
|--------|------|------|
| L1 dec_prep | 无 | AIV-only |
| L2 dec_ntt | **1 / 3** | 独立 launch |
| L3 dec_intt | **1 / 3** | 复用表，独立 launch |
| L4 enc_prep | 无 | AIV-only：G/H |
| L5 enc_compute | **1 / 3 + 4=GATE** | Encrypt 全链 |

永禁 **5 / 7**、SoftSync、Wait 环 SyncAll。`BLOCK_DIM=1`。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `dk.bin` | 3168B = dk_pke‖ek‖h‖z |
| `c.bin` | 1568B |
| `zetas` / `gammas` / `mat_*` | LUT / 极轻 Cube |
| **无** `K` / `m'` | 禁预喂最终共享密钥与明文 |

## 设备 / Host 输出（`output/`）

`K.bin`（主验收）、`m_prime` / `c_prime` / `K_prime`、`trace_dec` / `trace_enc`、`out` magic `0x543F001A`。

## Golden

`golden_K.bin` ← `liboqs_kem_ref decaps`；缺库 **BLOCKED**。

## 硬锁 / 禁令

- 五 launch + Host mid-sync；Decrypt 与 Reenc **分 workspace**
- 禁抄 alg15/alg20/alg21/decrypt/decaps/encrypt/encaps/l18_l19/frozen
- 拒绝分支 Host J（tiny_sha3）；CT 选路可后刀
