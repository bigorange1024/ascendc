# STATUS — fix-f203-alg15-pke-decrypt-correctness-k4

FIPS 203 **Alg.15 PKE Decrypt**（ml_kem_1024 / k=4）；Host 仅 I/O + LUT 搬运，密码学全在 device AscendC。

| 项 | 值 |
|---|---|
| **Gate** | G4 全链 `dk_pke + c → m` |
| **I/O** | dk 1536B · c 1568B · m 32B |
| **SEED_D** | 20260619（与 Encrypt round-trip） |

**跨探针闭环**：[`scripts/roundtrip_pke_encrypt_decrypt.sh`](../../scripts/roundtrip_pke_encrypt_decrypt.sh) — device c→m；CPU+SIM max=0（2026-06-30）。

**liboqs L2**：[`scripts/liboqs_pke_vs_ascendc.sh`](../../scripts/liboqs_pke_vs_ascendc.sh) — Decrypt **m** vs liboqs **CPU+SIM max=0**（2026-07-01；上游 Encrypt `Compress_5` 修复后）。详 [`docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](../../docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md)。

## 验收

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | G1–G4 max=0 |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | G1–G4 max=0；tick ~427k |

## Launch 编排（2 次 aclrtLaunchKernel）

| # | 核 | 说明 |
|---|-----|------|
| 1 | `f203_decrypt_g4_prep` | unpack + decode ŝ（AIV 标量，MIX 占位） |
| 2a | `f203_decrypt_g4_chain_ntt` | NTT + su_dot + pad（Launch-2 内第 1 个 kernel，`sync`） |
| 2b | `f203_decrypt_g4_chain_intt` | INTT + extract m（Launch-2 内第 2 个 kernel） |

prep 须与 NTT 分 launch（否则 SIM 上 û 错）；NTT 与 INTT 须分 kernel（同 launch 内二次 launch + sync，否则 m 错）。

## 备注

- 每个 kernel 后 `aclrtSynchronizeStream`（对齐 Encrypt G5）。
- 旧 6 核 / 单 launch `g4_full` 已废弃。
