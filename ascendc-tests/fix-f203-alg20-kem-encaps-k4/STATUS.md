# STATUS — fix-f203-alg20-kem-encaps-k4

FIPS 203 **Algorithm 20 `ML-KEM.Encaps(ek)`**（**ml_kem_1024 / k=4**）；经 **Alg.17 `Encaps_internal`** 调用 vendor **Alg.14 Encrypt**。

| 项 | 值 |
|---|---|
| **阶段** | **G3 CPU PASS**（2026-07-02）；SIM 待办公室复验 |
| **参数集** | ml_kem_1024（k=4） |
| **公钥 input** | **`ek_kem.bin` 1568B** ← [`fix-f203-alg19-kem-keygen-k4`](../fix-f203-alg19-kem-keygen-k4/) `output/`（同 `SEED_D`；**不**内嵌 KeyGen） |
| **I/O（锁定）** | `c` **1568B** · `K` **32B** · Host 仅 `seed_d`（4B）驱动 device `m` |
| **SEED_D** | **20260619**（与 KeyGen 一致） |

**Alg.20 约束**：`m` device UB 生成、不导出；**禁止**落盘 `a_hat` 等中间张量；见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §4.2。

## 上游依赖

| 段 | 来源 | 状态 |
|----|------|------|
| Alg.19 KeyGen | [`fix-f203-alg19-kem-keygen-k4`](../fix-f203-alg19-kem-keygen-k4/) | **PASS**（供给 `ek_kem`） |
| Alg.14 Encrypt G5（vendor） | [`frozen-fix-f203-alg14-pke-encrypt-correctness-k4`](../frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/) → `vendor/pke_encrypt/` | 冻结快照；stable **布局不兼容**；重构见 **T19a**（`qa/TODO.md`） |
| 设备 SHA3 | [`library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`](../../library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp) | KeyGen/Encrypt 已用 |

## 验收

| 模式 | 命令 | 状态 |
|------|------|------|
| CPU | `KEM_ENCAPS_VERIFY=1 bash run.sh -r cpu -v Ascend910B4` | **PASS** c/K max=0 |
| SIM | `KEM_ENCAPS_SKIP_REBUILD=1 KEM_ENCAPS_VERIFY=1 bash run.sh -r sim -v Ascend910B4` | **待复验**（早前 tick **1029406** PASS） |
| L2 liboqs 分项 | `bash scripts/liboqs_kem_encaps_batch.sh` | **CPU×2 冒烟 PASS**（2026-07-03）；默认 CPU×10+SIM×1，固定 stash `ek` + `KEM_ENC_EXT_SEED=1` 旁路 `m` |
| L2 liboqs 全链 | `bash scripts/liboqs_kem_vs_ascendc.sh` encaps 段 | 脚本待扩 |

## run.sh 要点

- `KEM_ENCAPS_SKIP_REBUILD=1` — 跳过 cmake（日常 smoke）
- `KEM_ENCAPS_KAT=1` — kat 批测 quiet（尺寸校验，log 重定向）
- `KEM_ENC_EXT_SEED=1` — **仅 kat**；读 `input/encaps_seed.bin` 旁路 `m`（见 INTEGRATION_PLAN §4.2.1）
- **Build profile 隔离（2026-07-03）**：默认 `KEM_ENC_EXT_SEED=0` 走 `build_prod_<mode>/out_prod_<mode>`；kat 旁路 `KEM_ENC_EXT_SEED=1` 走 `build_extseed_<mode>/out_extseed_<mode>`。`KEM_ENCAPS_BUILD_PROFILE=<name>` 可显式覆盖。
- `CMAKE_BUILD_JOBS=2` — 默认（WSL 勿满核）
- 缺 `EK_KEM_SRC` → exit 2（不拉 alg19）

CPU 回归：`roundtrip` prod 建库后，`liboqs_kem_encaps_batch.sh` CPU×1 走 extseed profile PASS；回到 `roundtrip_kem_encaps.sh -r cpu` 仍 `skip rebuild (profile=prod)`，无互相污染。

## 备注

- 实现方案见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)。
- Decaps（Alg.21）**不在本目录**。
