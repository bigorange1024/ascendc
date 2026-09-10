# LAYOUT — RB-T29 Decaps 3-launch

**L1** `dec_decrypt`（MIX：prep → SyncAll → NTT+dot → INTT+extract）→ m' →
**L2** `enc_prep`（AIV）→ **L3** `enc_compute`（MIX）→ c'/K'

| Launch | Flag | 备注 |
|--------|------|------|
| L1 dec_decrypt | **1/3** 复用两轮；前缀 SyncAll | basename 唯一 |
| L2 enc_prep | 无 | AIV-only |
| L3 enc_compute | **1/3+4** | 对齐 T28 Reenc |

验收：`K.bin`≡liboqs；`out` magic `0x543F001D`；TRACE PREP→NTT→INTT→EXTRACT。
