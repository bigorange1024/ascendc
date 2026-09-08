# LAYOUT — RB-T27 设备 Encaps→Decaps 往返

## 数据流（一句话）

Host：liboqs KeyGen→ek/dk + 固定 m →
**L0–L1 Encaps**（T23）：G(m‖H(ek))→Encrypt→`c`/`K_enc` →
**L2–L4 Decrypt**（T25）：→`m'` →
**L5–L6 Reenc**（T22/23）：G(m'‖H(ek))→Encrypt→`c'`/`K'` →
Host：`c'≡c ? K' : J(z‖c)` → `K_dec`；验收 `K_dec≡K_enc`（+ liboqs 交叉）。

## Flag

| Launch | Flag | 说明 |
|--------|------|------|
| L0 enc_prep | 无 | AIV-only：G/H（Encaps） |
| L1 enc_compute | **1 / 3 + 4=GATE** | Encrypt 全链 |
| L2 dec_prep | 无 | AIV-only |
| L3 dec_ntt | **1 / 3** | 独立 launch |
| L4 dec_intt | **1 / 3** | 独立 launch |
| L5 enc_prep | 无 | AIV-only：G/H（Reenc） |
| L6 enc_compute | **1 / 3 + 4=GATE** | Encrypt 全链 |

永禁 **5 / 7**、SoftSync、Wait 环 SyncAll。`BLOCK_DIM=1`。

## X12 写出

| 产物 | 写出方式 |
|------|----------|
| h / K / coins | UB + `DataCopy`（`reenc/encaps_g_device.hpp`） |
| ek / ρ 镜像 | UB + `DataCopy`（`reenc/prep_custom.cpp`） |
| c | UB + `DataCopy`（pack，既有） |
| ŝ / u / v | 直读 `dkIn`/`cIn` H2D + `DataCopy`（`decrypt/prep_device_math.hpp`；**禁 SetValue 镜像**） |
| m' | UB + `DataCopy`（`decrypt/intt_device_math.hpp`；**禁 SetValue**） |

禁依赖 `GlobalTensor::SetValue` 写业务 GM（密钥/密文/明文/中间镜像）。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `ek.bin` / `dk.bin` / `m.bin` | liboqs KeyGen + 固定 m |
| `zetas` / `gammas` / `mat_*` | LUT / 极轻 Cube |
| **无** `c` / `K` / `m'` | 禁预喂密文与最终密钥 |

## 设备 / Host 输出（`output/`）

`c_encaps` / `K_encaps` / `K_decaps` / `K.bin`、`m_prime` / `c_prime` / `K_prime`、
`trace_encaps` / `trace_dec` / `trace_reenc`、`out` magic `0x543F001B`。

## Golden

`golden_K` / `golden_c` ← liboqs Encaps（同 m/ek）；缺库 **BLOCKED**。
主验收：`K_decaps ≡ K_encaps`（设备往返）。

## 硬锁 / 禁令

- 七 launch + Host mid-sync；Encaps / Decrypt / Reenc **分 workspace**
- 复用自研树 RB-T23 LAYOUT/握手 + RB-T26 decrypt/reenc；禁抄 alg15/20/21/decrypt/decaps/encrypt/encaps/l18/frozen
