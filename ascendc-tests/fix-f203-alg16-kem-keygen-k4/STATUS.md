# STATUS — fix-f203-alg16-kem-keygen-k4

FIPS 203 **Algorithm 16 ML-KEM.KeyGen**（**ml_kem_1024 / k=4**）；**规划阶段**，代码未开工。

| 项 | 值 |
|---|---|
| **阶段** | 仅 `INTEGRATION_PLAN.md` + 本文件；**待家里 Agent 实现** |
| **参数集** | ml_kem_1024（k=4）；与 PKE 探针 / stable 一致 |
| **I/O（锁定）** | `ek_kem` **1568B** · `dk_kem` **3168B**（liboqs 展开：`dk_pke‖ek‖H(ek)‖z`） |
| **SEED_D** | **20260619**（与 PKE liboqs 交叉验证同源，待实现时锁定） |

## 上游依赖（已 PASS）

| 段 | 来源 | 状态 |
|----|------|------|
| Alg.13 PKE KeyGen | [`examples/stable/stable-mlkem-f203-pke-keygen-k4`](../../examples/stable/stable-mlkem-f203-pke-keygen-k4/) | stable + liboqs ek/dk_pke max=0 |
| 设备 SHA3-256 | [`library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`](../../library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp) | KeyGen/Alg.7 已用 |
| 探针对照 | [`pass-fix-f203-alg13-device-keygen-k4`](../pass-fix-f203-alg13-device-keygen-k4/) | 调试 / vendor 源 |

## 验收（目标）

| 模式 | 命令 | 目标 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | G1–G3 max=0 |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | 同上；无 507000 |
| L2 liboqs | `bash scripts/liboqs_kem_vs_ascendc.sh`（仓库根，**待建**） | ek/dk **3168+1568** max=0 |

## 备注

- 实现方案见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)。
- Encaps/Decaps（Alg.17/18）**不在本目录**；后续独立探针。
