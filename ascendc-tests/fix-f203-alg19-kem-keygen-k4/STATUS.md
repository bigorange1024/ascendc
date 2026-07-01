# STATUS — fix-f203-alg19-kem-keygen-k4

FIPS 203 **Algorithm 19 `ML-KEM.KeyGen()`**（**ml_kem_1024 / k=4**）；经 **Alg.16 `KeyGen_internal`** 完成 PKE+KEM 拼装。

| 项 | 值 |
|---|---|
| **阶段** | **G3 CPU+SIM PASS**（2026-07-01）；3 launch：prep \| mmad \| kem_finish |
| **SIM tick** | **742558**（含 KEM 尾段） |
| **参数集** | ml_kem_1024（k=4）；与 PKE 探针 / stable 一致 |
| **I/O（锁定）** | `ek_kem` **1568B** · `dk_kem` **3168B**（liboqs 展开：`dk_pke‖ek‖H(ek)‖z`） |
| **SEED_D** | **20260619**（Host 仅 4B `seed_d`；`d`/`z` 由 device UB 派生） |

**Alg.19 约束**：`d`/`z` device UB 生成、不导出；见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §4.2。

## 上游依赖（已 PASS）

| 段 | 来源 | 状态 |
|----|------|------|
| Alg.13 PKE KeyGen | [`examples/stable/stable-mlkem-f203-pke-keygen-k4`](../../examples/stable/stable-mlkem-f203-pke-keygen-k4/) | stable + liboqs ek/dk_pke max=0 |
| 设备 SHA3-256 | [`library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`](../../library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp) | KeyGen/Alg.7 已用 |
| 探针对照 | [`pass-fix-f203-alg13-device-keygen-k4`](../pass-fix-f203-alg13-device-keygen-k4/) | 调试 / vendor 源 |

## 验收（目标）

| 模式 | 命令 | 目标 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | **PASS** ek/dk max=0（`KEM_KEYGEN_VERIFY=1`） |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**；tick **742558**；无 507000 |
| L2 liboqs | `bash scripts/liboqs_kem_vs_ascendc.sh` | CPU+SIM **PASS** max=0 |

## 备注

- 实现方案见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)。
- Encaps/Decaps（Alg.17/18）**不在本目录**；后续独立探针。
