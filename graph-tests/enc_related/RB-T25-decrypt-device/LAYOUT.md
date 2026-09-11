# LAYOUT — RB-T25 Alg.15 Decrypt 设备多 launch

## 数据流（一句话）

Host 预装 `dk_pke`+`c`+ζ/γ/mat →
**Launch1** prep AIV：直读 H2D 的 dk/c（DataCopy 字节入 UB）→ŝ/u/v → mid-sync →
**Launch2** MIX：û←NTT(u)、ŵ←⟨ŝ,û⟩ → mid-sync →
**Launch3** MIX：w←INTT(ŵ)、m←BE₁(Compress₁(v−w)) 经 UB+DataCopy 写出。

NPU 注意：禁依赖 `GlobalTensor::SetValue` 写密文/明文 GM（CPU 孪生可假绿）。

## Flag

| Launch | Flag | 说明 |
|--------|------|------|
| L1 prep | 无 | AIV-only |
| L2 NTT | **1 / 3** | AIC Wait(1)→Cube→Set(3)；AIV Set(1)→Wait(3)→NTT+dot |
| L3 INTT | **1 / 3**（复用表，独立 launch） | 同上外壳；AIV0 INTT+extract |

永禁 **5 / 7**、SoftSync、Wait 环 SyncAll。本刀**不用** GATE(4)（NTT/INTT 已拆 launch）。

## BLOCK_DIM

`blockDim=1`（Host 硬锁）。

## Host 输入（`input/`）

| 文件 | 说明 |
|------|------|
| `dk_pke.bin` | 1536B ByteEncode₁₂(ŝ) |
| `c.bin` | 1568B = c₁(1408)‖c₂(160) |
| `zetas.bin` / `gammas.bin` | NTT/MultiplyNTTs 表 |
| `mat_a.bin` / `mat_b.bin` | 极轻 Cube |
| **无** `m.bin` | 禁预喂最终明文 |

## 设备输出（`output/`）

`m.bin`（主验收）、`out.bin`（magic `0x543F0019`）、`trace.bin`、中间量 ŝ/u/v/û/ŵ/w。

## Golden

`golden_m.bin` ← `scripts/liboqs_pke_ref decrypt`；缺库 **BLOCKED**。

## 硬锁 / 禁令

- 三 launch + Host `aclrtSynchronizeStream`（CPU：`ICPU_RUN_KF` 序贯）
- 未采用生产笔记「1-kernel + SoftSync + GATE 8」
- 禁抄 alg15 / decrypt / encrypt / encaps / decaps / l18_l19 / frozen
