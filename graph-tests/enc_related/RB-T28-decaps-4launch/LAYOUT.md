# LAYOUT — RB-T28 Alg.21 Decaps 4-launch

## 数据流

Host 预装 `dk_kem`+`c`+ζ/γ/mat →
**L1** `dec_prep`（AIV）→ **L2** `dec_ntt_intt`（融合 MIX：NTT+dot → INTT+extract）→ m' →
**L3** `enc_prep`（AIV）→ **L4** `enc_compute`（MIX）→ c'/K' →
Host：`c'≡c ? K' : J(z‖c)` → `K[32]`。

## Flag

| Launch | Flag | 说明 |
|--------|------|------|
| L1 dec_prep | 无 | AIV-only |
| L2 dec_ntt_intt | **1 / 3** 复用两轮 | 对齐 D08；永禁 5/7 |
| L3 enc_prep | 无 | AIV-only：G/H |
| L4 enc_compute | **1 / 3 + 4=GATE** | Encrypt 全链 |

`BLOCK_DIM=1`。Decrypt 与 Reenc **分 workspace**。

## 输出

`K.bin`（主验收）、`m_prime` / `c_prime` / `K_prime`、`trace_dec` / `trace_enc`、`out` magic `0x543F001C`。
