# INTEGRATION_PLAN — fix-f203-alg20-kem-encaps-k4

**定位**：`ascendc-tests/` **Alg.20 `ML-KEM.Encaps(ek)` 设备全链正确性探针**（**ml_kem_1024 / k=4**）。对外实现 FIPS 203 Algorithm 20；内部经 **Alg.17 `Encaps_internal(ek,m)`** 调用已验收 **Alg.14 Encrypt**；**非** `examples/` 交付。

**自包含约束**：[`SELF_CONTAINED.md`](SELF_CONTAINED.md) · 工程通则 [`用例自包含与设备全链约束.md`](../../docs/engineering/用例自包含与设备全链约束.md)。

**用户锁定原则（2026-07-02）**：

| 项 | 约定 |
|----|------|
| **密码学位置** | KEM 增量（`m` 采样、`H(ek)`、`G(m‖H(ek))`、输出 `K`）与 **Alg.14 Encrypt 全链**均在 **AI Core**；禁止 Host 胶水冒充 `c`/`K` |
| **封装公钥 `ek`** | **读 KEM KeyGen 产出**：[`fix-f203-alg19-kem-keygen-k4`](../fix-f203-alg19-kem-keygen-k4/) 的 `output/ek_kem.bin`（1568B，与 `ek_PKE` 同布局）；**非**单独再跑 PKE KeyGen、**非** Host 调 liboqs 写 `ek` |
| **PKE Encrypt 来源** | Alg.14 G5 → [`fix-f203-alg14-pke-encrypt-correctness-k4`](../fix-f203-alg14-pke-encrypt-correctness-k4/)；本探针 **vendor 复制** launch/kernel，**不** `#include` alg14 路径 |
| **KeyGen 关系** | 与 alg19 **解耦目录**；验收时 **同 `SEED_D`** 先 KeyGen 再 Encaps，Encaps 仅消费 `ek_kem.bin` |
| **参数集表述** | 全文 **ml_kem_1024（k=4）** |
| **SHA3** | 设备路径经 `library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`；`H` = SHA3-256，`G` = SHA3-512 |
| **Alg.20 随机性 `m`** | **在 device AscendC 生成**（首版 **UB 驻留**）；**禁止**导出/落盘 `m` 本体；可复现验收 Host 仅给 **`SEED_D`**（4B，与 KeyGen 同域分离惯例） |

---

## 1. 目标与不变量

### 1.1 FIPS 203：Alg.20 → Alg.17（相对 PKE Encrypt 的增量）

**对外生产路径实现 Algorithm 20** `ML-KEM.Encaps(ek)`：

```text
m ←$ B^32                              // Alg.20 行 1：device AscendC（UB 驻留，不导出）
(K, c) ← ML-KEM.Encaps_internal(ek, m) // Alg.17
```

**Alg.17 Encaps_internal**（给定 `ek`、`m`）：

```text
(K, r) ← G(m ‖ H(ek))                  // G = SHA3-512 → 64B：K[0:32] ‖ r[32:64]
c ← K-PKE.Encrypt(ek, m, r)            // Alg.14；r 即 Encrypt 的 coins
return (K, c)
```

| 不变量 | 说明 |
|--------|------|
| **设备全链** | 默认 `run.sh`：`ek_kem` → 设备采 `m` → `H`+`G` → vendor Encrypt → `output/c.bin` + `output/K.bin` |
| **公钥来源** | `input/ek_kem.bin` 由 **`gen_data` 从 alg19 `output/ek_kem.bin` 复制**（同 `SEED_D`）；不内嵌 KeyGen launch |
| **拼装来源** | alg14 G5 **vendor 到** `vendor/pke_encrypt/` + `library/shared`；禁止跨探针 `#include` |
| **Golden** | `scripts/host_golden/` **仅** `KEM_ENCAPS_VERIFY=1`；liboqs 走仓库 `scripts/liboqs_kem_vs_ascendc.sh` encaps 段（**待扩**） |
| **Launch** | **单进程、单 ACL session**（对齐 alg14 G5 / alg19 KeyGen）；禁止子进程调 alg14 `run.sh` |
| **SIM** | AIV `func_key ≤ 4`（已验收上限 5 核）；KEM 头**优先并入** `prep_re`，避免新增第 6 个 AIV 核 |

### 1.2 生产 I/O（锁定）

| 文件 | 尺寸 | 语义 |
|------|------|------|
| `input/ek_kem.bin` | **1568** | 来自 alg19 KeyGen（同 `SEED_D`）；= `ByteEncode₁₂(t̂) ‖ ρ` |
| `input/seed_d.bin` | 4B uint32 LE | 与 KeyGen/Encrypt 探针相同；默认 **20260619**；仅用于 device `DerandMFromSeedD` |
| `output/c.bin` | **1568** | ML-KEM 密文（= PKE Encrypt 输出） |
| `output/K.bin` | **32** | 共享秘密（FO 导出；**非** `m` 本体） |

**LUT**：vendor Encrypt 链所需 `lut_*` / ROM 与 alg14 一致，由 `gen_data` / `prepare_production_input` 生成。

### 1.3 与 alg19 的协作（非本目录职责）

```text
# 标准验收序（仓库级或手工）
SEED_D=20260619
cd ascendc-tests/fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg20-kem-encaps-k4
# gen_data 复制 ../fix-f203-alg19-kem-keygen-k4/output/ek_kem.bin → input/
bash run.sh -r cpu -v Ascend910B4
```

环境变量（实现阶段）：

| 变量 | 默认 | 说明 |
|------|------|------|
| `EK_KEM_SRC` | `../fix-f203-alg19-kem-keygen-k4/output/ek_kem.bin` | `gen_data` 复制公钥源 |
| `SEED_D` | `20260619` | KeyGen / Encaps 须一致 |

---

## 2. 已验收上游（拼装清单）

| **Encaps_internal 段** | FIPS | 上游 | 本目录策略 |
|------------------------|------|------|------------|
| **L0** `H(ek)` | Hash | `fips203_device_sha3.hpp` → `Sha3OneShot(h, 32, ek, 1568)` | 与 alg19 `KemKgFinish` 同型；可抽 `kem/f203_kem_hash_ek.hpp` |
| **L1** 采 `m` | Alg.20 行 1 | device UB：`DerandMFromSeedD(seed_d, m)` | 域分离前缀独立（见 §4.2） |
| **L2** `G(m‖h)` | Alg.17 行 1 | `Sha3OneShot(Kr, 64, m‖h, 64)` | 拆 `K[0:32]`、`r[32:64]`；`r` → Encrypt `coins` |
| **L3** PKE Encrypt | Alg.14 | alg14 G5 vendor | `ek`+`m`+`r` → `c`；SIM 参考 tick **~922441** |
| **L4** 写输出 | Alg.20 | GM | `c.bin`、`K.bin` |

**vendor 同步源**：[`fix-f203-alg14-pke-encrypt-correctness-k4`](../fix-f203-alg14-pke-encrypt-correctness-k4/)（须含 **Compress_5 `(1<<26)`** 修复版 pack）。

---

## 3. 分阶段 Gate

| Gate | 设备路径 | 验收 |
|------|----------|------|
| **G0** | launch 壳 + vendor_sync | kernel 正常结束 |
| **G1** | L3：vendor Encrypt alone（fixture `ek`/`m`/`coins`） | vs alg14 同输入 `c` max=0 |
| **G2** | L0+L1+L2：`H`+`m`+`G` | vs `host_golden` 中间量 |
| **G3** | L3+L4：Encaps_internal 全链 | `c`+`K` max=0 |
| **G4** | Alg.20 生产 I/O | 默认 `run.sh`；`KEM_ENCAPS_VERIFY=0` |

**过渡规则**：G3 双模式 PASS 后冻结 G0–G2 分段 env（对齐 alg14 G5 治理）。

---

## 4. Launch 编排（首版方案）

### 4.1 原则

1. **一次 `aclInit` / 一个 `aclrtStream`** 跑完 KEM 头 + vendor Encrypt G5 全序列。
2. **func_key 预算**：alg14 已占满 5 个 AIV 核；**禁止**默认路径新增独立 `f203_kem_enc_init` AIV 核（除非 SIM 实测合并后仍 ≤5）。
3. **首选融合点**：在 **`f203_encrypt_prep_re` 之前**（host 或单段 UB 标量）完成 `m`+`H(ek)`+`G`→写 `coins_gm`；`m_gm` 供 Encrypt 明文路径消费。若 SIM 不允许 host 参与，则 **扩 prep_re 入口**：先 UB 内 `DerandM`+`G`，再 CBD 采 `r/e₁/e₂`（`coins` 来自 `G` 后半而非 Host）。

### 4.2 `m` 设备采样契约（**用户锁定**）

| 约束 | 说明 |
|------|------|
| **驻留** | `m[32]` 在 UB 完成域分离 SHA3（`DerandMFromSeedD`）；与 alg19 `DerandZFromSeedD` 同型 |
| **禁止** | D2H `m`；`output/m.bin`；Host 预填 32B `encaps_seed` 进生产 `run.sh` |
| **可复现** | Host 仅 `seed_d.bin`；消息前缀建议 `exp-mlkem-f203-kem-encaps-k4:SEED_M=` + decimal(`SEED_D`)（实现前用 `liboqs_kem_fixture` 锁定期望 `m`） |
| **`G` 输入** | `buf = m[32] ‖ h[32]`，共 64B → SHA3-512 → `kr[64]`；`K=kr[0:32]`，`coins=kr[32:64]` |

### 4.2.1 测试旁路 A（`KEM_ENC_EXT_SEED`，**仅 kat，默认关**）

| 项 | 说明 |
|---|---|
| **目的** | Encaps 分项 kat：固定 stash `ek`，每轮 `os.urandom(32B)` 作 `m`，与 liboqs `encaps_derand(ek,m)` 对拍 `c/K` |
| **机制** | 宏开：`input/encaps_seed.bin`（32B）经 GM 注入 `m`；**仍** device 内 `H(ek)` + `G(m‖H(ek))` → `coins/K`（**禁止** host 预填 `coins`） |
| **脚本** | `scripts/kat_liboqs_kem_encaps.py` + `liboqs_kem_encaps_batch.sh`；前置 `output/kem_keypair_stash/`（`kem_keypair_stash_bootstrap.sh`） |
| **生产** | `KEM_ENC_EXT_SEED=0`（默认）；`run.sh` / CMake 无影响 |


```text
aclInit / CreateStream
  │
  ├─ [可选] Launch-0: f203_kem_enc_marker
  │
  ├─ [KEM 头 — 实现二选一，SIM 前不得拍板]
  │    A) 单段 AIV `f203_kem_enc_init`：读 ek_gm + seed_d → UB m,h,K,r → 写 coins_gm,m_gm,K_gm
  │       （仅当 nm 后 AIV 核仍 ≤5 且 func_key≤4）
  │    B) 并入 prep_re：入口先 DerandM+G，再原 CBD(coins 已由 G 填充)
  │
  ├─ vendor Encrypt G5 序列（与 alg14 一致）：
  │    prep_a_hat → prep_re → ntt_r → decode_t_hat → sync → at_r5 → intt → g4_noise → pack
  │    → c_gm[1568]
  │
  └─ D2H → output/c.bin, output/K.bin
aclFinalize
```

**同步点**：`at_r5` 前 `aclrtSynchronizeStream` 与 alg14 相同；KEM 头与 `prep_a_hat` 之间是否需要 sync 由 G2 分段测定。

---

## 5. 目录结构（实现阶段）

```text
fix-f203-alg20-kem-encaps-k4/
├── INTEGRATION_PLAN.md
├── STATUS.md
├── SELF_CONTAINED.md
├── CMakeLists.txt
├── run.sh
├── main_kem_encaps.cpp
├── f203_kem_enc_layout.h
├── vendor/
│   └── pke_encrypt/         # 自 alg14 G5 vendor_sync 复制
├── kem/
│   ├── f203_kem_enc_derand_ub.hpp    // DerandMFromSeedD
│   ├── f203_kem_enc_init.hpp         // H(ek)+G(m‖h)→K,r
│   └── f203_kem_enc_init_entry.cpp   // 若采用独立 Launch-A
├── scripts/
│   ├── gen_data.py                   // 复制 ek_kem + seed_d + LUT
│   ├── prepare_production_input.py
│   ├── vendor_sync_from_alg14_encrypt.sh
│   └── host_golden/
│       ├── golden_c_K.py
│       └── gate_g1_encrypt.py …
└── cmake/
    └── encaps/
        └── CMakeLists.txt
```

---

## 6. 验收命令（目标）

```bash
# 前置：同 SEED_D 生成 ek
cd ascendc-tests/fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4

# 探针内
cd ../fix-f203-alg20-kem-encaps-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# L2 liboqs（仓库根，encaps 段待扩）
bash scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4   # 实现后含 encaps
```

---

## 7. 与相邻探针的边界

| 探针 | 算法 | 关系 |
|------|------|------|
| [`fix-f203-alg19-kem-keygen-k4`](../fix-f203-alg19-kem-keygen-k4/) | Alg.19 KeyGen | **供给 `ek_kem.bin`**（本探针 input） |
| **本目录** | Alg.20 Encaps | 消费 `ek_kem`；产出 `c`/`K` |
| `fix-f203-alg21-kem-decaps-k4`（待建） | Alg.21 Decaps | 消费 `dk_kem`+`c`；vendor alg15 |

**不在 `examples/incubating/exp-*` 首版**。

---

## 8. 实现顺序（建议）

1. `scripts/vendor_sync_from_alg14_encrypt.sh` + `gen_data.py`（**仅复制 alg19 `ek_kem`** + seed + LUT）+ 空 `run.sh`/`CMakeLists` 壳（G0）。
2. vendor Encrypt G1 vs alg14 同 fixture max=0。
3. `DerandMFromSeedD` + `liboqs_kem_fixture` 锁 `m`（扩 fixture encaps 段）。
4. 设备 `H+G` G2；选定 §4.3 融合点 A/B。
5. G3 `c`+`K` + `KEM_ENCAPS_VERIFY=1` CPU+SIM。
6. 扩 `scripts/liboqs_kem_vs_ascendc.sh` / `liboqs_kem_ref.c` encaps 子命令。
7. 刷新 `STATUS.md`、`docs/notes/`（Encaps 原理，可新建 note）、当日 `qa/`。

---

## 9. 风险与阻塞

| 风险 | 缓解 |
|------|------|
| **func_key 爆炸** | 优先方案 B（并入 prep_re）；禁止先加独立核再救火 |
| vendor alg14 与 pass 漂移 | vendor_sync 清单 + G1 回归 |
| `G` 与 liboqs `encaps_derand` 不一致 | 先 fixture 锁 `m`/`K`/`coins`/`c` |
| alg19 `ek` 未生成 | `gen_data` 检查 `EK_KEM_SRC` 存在性；CI 串 KeyGen→Encaps |
| Encrypt tick 回归 | 以 alg14 SIM tick **922441** 为基线；KEM 头增量单独记 |

---

## 10. 参考

| 文档 | 链接 |
|------|------|
| Alg.19 KeyGen 探针 | [`fix-f203-alg19-kem-keygen-k4/INTEGRATION_PLAN.md`](../fix-f203-alg19-kem-keygen-k4/INTEGRATION_PLAN.md) |
| Alg.14 Encrypt G5 | [`fix-f203-alg14-pke-encrypt-correctness-k4/INTEGRATION_PLAN.md`](../fix-f203-alg14-pke-encrypt-correctness-k4/INTEGRATION_PLAN.md) |
| KEM KeyGen 原理 | [`docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md`](../../docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md) |
| func_key / SIM | [`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md) |
| 讨论 | [`qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md`](../../qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md) §4 |
