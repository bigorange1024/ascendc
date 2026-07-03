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
| 旁路 A 批测 | `bash scripts/liboqs_kem_keygen_batch.sh` | **11/11 PASS**（CPU×10+SIM×1，os.urandom 相同随机字节）2026-07-03 |

## KEM_KG_EXT_SEED 旁路 A 正确性批测（test-only · 2026-07-03）

kem.keygen 只吃随机性。为验证 KeyGen 核在**任意随机**下正确，令 liboqs `keypair_derand` 与本探针吃**逐字节相同**的 `os.urandom` 64B `kem_seed = d‖z`：

- **宏 `KEM_KG_EXT_SEED`（默认 0）**：=1 时 prep 取 `kem_seed[0:32]` 作 `d`、finish 取 `[32:64]` 作 `z`（不经 SEED_D 派生）；生产/默认路径与 stable/vendor **零改动**。见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §4.2.1。
- **脚本**：`scripts/liboqs_kem_keygen_fixture.py`（随机 kem_seed + liboqs golden）、`scripts/liboqs_kem_keygen_batch.sh`（CPU×10+SIM×1 驱动，复用 `liboqs_kem_vs_ascendc_verify.py --stage keygen`）。
- **结果**：`ek_kem`/`dk_kem` 与 liboqs 逐字节 max=0，**11/11 PASS**。
- **CMake 教训**：SIM/NPU 宏须 `ascendc_compile_definitions`；用 `target_compile_definitions` 只对 CPU 生效，SIM 会退回 seed_d 分支致全错（已修）。

## run.sh profile 隔离（2026-07-03）

为避免生产/round-trip 与 liboqs kat 旁路 A 来回切换时共用 `build/`、`out/`，`run.sh` 已按 `profile × RUN_MODE` 隔离构建产物：

| profile | 触发 | 目录 |
|---------|------|------|
| `prod` | 默认 `KEM_KG_EXT_SEED=0`（生产 / round-trip） | `build_prod_<cpu|sim>` / `out_prod_<cpu|sim>` |
| `extseed` | `KEM_KG_EXT_SEED=1`（liboqs keygen kat） | `build_extseed_<cpu|sim>` / `out_extseed_<cpu|sim>` |

- `KEM_KEYGEN_BUILD_PROFILE=<name>` 可显式覆盖；`-p` 仍可覆盖 install prefix。
- CPU 回归：`prod → extseed kat → prod` 后，prod 再跑仍 `skip rebuild`；extseed 与 prod 互不污染。

## 待定位 flaky（2026-07-03，证据修正）

**现象（历史一次）**：build profile 隔离改造过程中观察到一次 CPU `verify FAIL`、紧接复跑 PASS。差异首字节在 `ek_kem[768]`；`dk_kem` 差异从 `2304=1536+768` 起——即 `dk_kem` 内嵌那份 `ek` 的第 768 字节，与 `ek_kem[768]` 是**同一份数据**。故错源唯一：**`t_hat` 后半（第 2、3 个 poly，`384×2=768`）**，由 Launch2 `mmad_custom`（即 PKE KeyGen / Alg.13 计算核）产出；`kem_finish` 只原样搬运，不改值。

**层归属**：错在 **PKE KeyGen 的 `t_hat` 计算**，不是 KEM 尾段。

**复现实验（2026-07-03）**：profile 隔离后，`prod_cpu` 做 4 轮「彻底重建后首跑」+ 4 轮「同二进制复跑」，共 8 次**全部 PASS**；此前连跑 5 次亦全 PASS。**目前无法在干净隔离环境重现。**

**判读（不再二选一断言）**：历史那次 FAIL 紧邻 `extseed↔prod` 共享 `build/` 结构切换，**高度怀疑是构建污染（双 entry `.o` 混链 / 半新旧 `.o`）的残留表现**；无法排除 `mmad_custom`（MIX，CPU 孪生多线程）低频运行时竞态。两类假设均未证实。**不加脚本重试掩盖**。若再现，定位法：先 `KEM_KEYGEN_FORCE_REBUILD=1` 干净跑排除污染，再对同二进制连跑批量采样确认是否运行时非确定，命中后沿 `src → a_hat → t_hat/ek_pke → kem_finish` 加 debug 切片二分。

## 备注

- 实现方案见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)。
- Encaps/Decaps（Alg.17/18）**不在本目录**；后续独立探针。
