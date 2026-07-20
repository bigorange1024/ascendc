# INTEGRATION_PLAN — fix-f203-alg19-kem-keygen-correctness-k4

**定位**：`ascendc-tests/` **Alg.19 `ML-KEM.KeyGen()` 设备全链正确性探针**（**ml_kem_1024 / k=4**）。对外实现 FIPS 203 Algorithm 19；内部经 **Alg.16 `KeyGen_internal(d,z)`** 拼装 PKE 与 KEM 尾段；**非** `examples/` 交付。

**自包含约束**：[`SELF_CONTAINED.md`](SELF_CONTAINED.md) · 工程通则 [`用例自包含与设备全链约束.md`](../../docs/engineering/用例自包含与设备全链约束.md)。

**用户锁定原则（2026-07-01）**：

| 项 | 约定 |
|----|------|
| **密码学位置** | KEM 增量步骤（`H(ek)`、采 `z`、拼 `dk_kem`）**全在 AI Core**；禁止 Host 胶水冒充生产输出 |
| **PKE 来源** | Alg.13 → **stable** [`stable-fips203-mlkem-pke-keygen-k4`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/)；本探针 **vendor 复制** launch/kernel，**不** `#include` stable 路径 |
| **Enc/Dec** | 本探针**不含**；后续 Alg.17/18 用 [`fix-f203-alg14`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) / [`fix-f203-alg15`](../../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) |
| **参数集表述** | 全文 **ml_kem_1024（k=4）**；勿与历史笔误「768」混用 |
| **SHA3** | 设备路径经 `library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`（语义对齐 tiny_sha3）；Host tiny_sha3 **仅** golden / `VERIFY=1` |
| **Alg.19 随机性 `d`/`z`** | **均在 device AscendC 上生成**（首版 **UB 驻留**）；**禁止**导出/落盘/GM 持久化 `d`/`z` 本体；仅允许消费派生量（如 `ρ`/`σ`→PKE 链、`H(ek)`、`dk_kem` 最终输出） |

---

## 1. 目标与不变量

### 1.1 FIPS 203：Alg.19 → Alg.16（相对 PKE 的增量）

**对外生产路径实现 Algorithm 19** `ML-KEM.KeyGen()`（非仅 Alg.16 internal 胶水）：

```text
d ←$ B^32          // Alg.19 行 1：device AscendC 生成（UB 驻留，不导出）
z ←$ B^32          // Alg.19 行 2：device AscendC 生成（UB 驻留，不导出）
(ek, dk) ← ML-KEM.KeyGen_internal(d, z)   // Alg.16
```

Alg.16 internal 在 PKE 段之后追加：

```text
ek  ← ek_PKE
dk  ← dk_PKE || H(ek) || z
```

**liboqs / PQClean 展开秘密钥**（本探针 **I/O 锁定**，与 `OQS_KEM_ml_kem_1024` 对拍）：

```text
dk_kem = dk_pke (1536B) || ek (1568B) || H(ek) (32B) || z (32B)  → 3168B
ek_kem = ek_PKE                                                    → 1568B
```

其中 `H` = FIPS 203 **Hash** = **SHA3-256**（32B 输出）。

| 不变量 | 说明 |
|--------|------|
| **设备全链** | 默认 `run.sh`：`seed_d` → 设备 Alg.13 全链 → 设备 `H(ek)` → 设备采 `z` → 设备拼接 → `output/` |
| **拼装来源** | stable KeyGen **vendor 到** `vendor/pke_keygen/` + `library/shared`；禁止跨探针 `#include` |
| **Golden** | `scripts/host_golden/` **仅** `KEM_KEYGEN_VERIFY=1`；liboqs 走仓库 `scripts/liboqs_kem_vs_ascendc.sh` |
| **Launch** | **单进程、单 ACL session**（对齐 Encrypt G5 / Decrypt G4）；禁止子进程调 stable `run.sh` |
| **SIM** | 遵守 func_key / 单 session；见 [`AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md) |

### 1.2 生产 I/O（锁定）

| 文件 | 尺寸 | 语义 |
|------|------|------|
| `input/seed_d.bin` | 4B uint32 LE | 与 PKE 探针相同；默认 **20260619** |
| `output/ek_kem.bin` | **1568** | ML-KEM 封装密钥 |
| `output/dk_kem.bin` | **3168** | ML-KEM 解封装密钥（liboqs 布局） |

---

## 2. 已验收上游（拼装清单）

| **KeyGen_internal 段** | FIPS | 上游 | 本目录策略 |
|-----------|------|------|------------|
| **L0** PKE KeyGen 全链 | Alg.13 | [`stable-fips203-mlkem-pke-keygen-k4`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/) | **vendor** `prep/` + `compute/` + `main_keygen` 编排到 `vendor/pke_keygen/`（**复制**，非 `#include`） |
| **L1** `H(ek)` | Hash | `fips203_device_sha3.hpp` → `Sha3OneShot(h, 32, ek, 1568)` | 新核 `f203_kem_kg_hash_ek` 或并入 L2 标量段 |
| **L2** 采 `z` | Alg.19 行 2 + Alg.16 行 3 | **device UB**：与 `d` 同 session 内生成；**禁止** D2H / `output/z.bin` / GM 持久化 |
| **L3** 拼 `dk_kem` | Alg.16 行 4 | GM `memcpy` 式拼接（AIV 标量） | `dk_pke‖ek‖h‖z` → `dk_kem.bin` |

**SHA3 分层（用户要求，实现时遵守）**：

```text
调用方（KEM kernel）
  → F203SeDeviceKeccak::Sha3OneShot / Shake256OneShot   // 稳定设备 API
       → 今：标量 Keccak-f[1600]（抄 tiny_sha3 语义）
       → 后：CANN 矢量 SHA3 实现替换本文件内部，调用方不改
```

批量 SHAKE PRF **若需要** 时走 `shake_xof_kernel` + `SHAKE256_RATE_BYTES`（与 KeyGen prep 同模式）；Alg.16 首版以 **单次 SHA3-256(H)** + 短 `z` 采样为主，不强制先上向量 SHAKE。

---

## 3. 分阶段 Gate

| Gate | 设备路径 | 验收 |
|------|----------|------|
| **G0** | launch 壳 | kernel 正常结束 |
| **G1** | L0：PKE KeyGen → `ek` + `dk_pke` GM | vs stable 同 `seed_d` 的 `ek_pke`/`dk_pke` max=0 |
| **G2** | L1+L2：`H(ek)`、`z` | vs `host_golden` 中间 bin |
| **G3** | L3：拼接 → `ek_kem.bin` + `dk_kem.bin` | **端到端 max=0**（1568+3168B） |
| **G4** | 生产 I/O | 默认 `run.sh` = G3；`KEM_KEYGEN_VERIFY=0` |

**过渡规则**：G3 双模式 PASS 后冻结 G0–G2 分段 env（对齐 Encrypt G5 治理）。

---

## 4. Launch 编排（首版方案）

### 4.1 原则

1. **一次 `aclInit` / 一个 `aclrtStream`** 跑完 G1–G3（或 G1 与 G2–G3 最多 **2 launch**，若 SIM 要求 PKE 与 KEM 尾部分离——实现时以 SIM 实测为准，须记入 `STATUS.md`）。
2. **vendor 的 PKE 段** 与 stable **launch 序、GM 布局、sync 点** 一致；改动的仅是尾段追加 KEM 步骤，不重写 NTT/内积。
3. PKE 中间 GM（`a_hat`、`src` 等）在单 session 内由 PKE 段写入、KEM 尾段只读 `ek`/`dk_pke` 最终缓冲。

### 4.2 `d` / `z` 设备采样契约（**用户锁定 · 2026-07-01**）

依据 **FIPS 203 Algorithm 19**：`d` 与 `z` **均须在 AI Core 上以 AscendC 方式生成**；计算过程**不得导出保存**（生产路径）。

| 约束 | 说明 |
|------|------|
| **驻留** | `d[32]`、`z[32]` 首版在 **UB**（`uint8_t d[32]` / `z[32]` 或 `TBuf`）完成派生/采样；PKE 段可沿用 stable `DerandFromSeedD` + `BuildRhoSigmaFromSeedD` 模式 |
| **禁止** | D2H `d`/`z`；`output/d.bin` / `output/z.bin`；`KEYGEN_DEBUG_DUMP` 类路径写出 `d`/`z`；Host 预填 64B `kem_seed` 冒充设备随机性 |
| **允许** | `ρ`/`σ` 等 **派生中间量** 按 PKE vendor 既有 GM 契约落盘（非 `d`/`z` 本体）；最终仅 D2H `ek_kem`/`dk_kem` |
| **API** | `F203SeDeviceKeccak::Sha3OneShot` / `Shake256OneShot`（`fips203_device_sha3.hpp`）；与 stable prep 同 Keccak 门面 |

**可复现验收（`SEED_D=20260619`）**：Host 仅写 `input/seed_d.bin`（4B uint32）；device 在 UB 内：

1. **`d`**：`DerandFromSeedD(seed_d, d)`（与 stable / liboqs PKE 已验路径一致）。
2. **`z`**：独立域分离的设备 SHA3/SHAKE（如 `DerandZFromSeedD`），**不得**复用 `d` 缓冲、不得 Host 侧先算再 H2D。

实现前用 `scripts/liboqs_kem_fixture.py`（待建）dump liboqs `keypair_derand` 在固定 `SEED_D` 下的 **64B `d‖z`**，锁定 `DerandZFromSeedD` 消息格式；L2 对拍以 liboqs 为准。

> **与旧 §4.2 方案 A/B 的关系**：不再二选一「Host 域分离 vs kem_seed 后半」；**唯一路径**为 device UB 双采样 + Alg.16 internal。

#### 4.2.1 `KEM_KG_EXT_SEED` 旁路 A（**test-only 例外 · 用户确认 2026-07-03**）

正确性交叉验证需让 liboqs `keypair_derand` 与本探针吃**逐字节相同的随机字节**（`os.urandom` 生成的 64B `kem_seed = d‖z`），以在任意随机性（非 SHA3(域分离串‖SEED_D) 像）上验证 KeyGen 核实现，语义近似 NIST KAT。

| 项 | 说明 |
|----|------|
| **编译宏** | `KEM_KG_EXT_SEED`（CMake cache，**默认 0**）；=1 时 CMake 用 probe-local prep 入口 [`kem/f203_keygen_prep_entry_extseed.cpp`](kem/f203_keygen_prep_entry_extseed.cpp) 替代 vendored 入口 |
| **数据面** | 宏开时 `seed_d_gm` 缓冲扩为 **64B**，承载 host `kem_seed`；prep 取 `[0:32]` 作 `d`（[`kem/f203_keygen_prep_extseed.hpp`](kem/f203_keygen_prep_extseed.hpp) `BuildKeygenPrepSinglePipeExtD`，ρ‖σ=G(d‖k)），finish 取 `[32:64]` 作 `z` |
| **生产隔离** | 宏默认关，生产/默认 `run.sh` 与 stable/vendor 源**零改动**（vendored prep 入口不动）；宏开仅由 `scripts/liboqs_kem_keygen_batch.sh` 置位并配 `KEM_KEYGEN_VERIFY=0` |
| **与 §4.2「禁止 host 预填 kem_seed」的关系** | §4.2 约束**生产路径**；本旁路为 **test-only 例外**，不改变生产 device UB 双采样契约，仅供交叉验证复现相同随机字节 |
| **验收** | `bash scripts/liboqs_kem_keygen_batch.sh` → CPU×10 + SIM×1，device `ek_kem`/`dk_kem` 与 liboqs `keypair_derand` 逐字节 max=0（2026-07-03 **11/11 PASS**） |
| **CMake 关键** | SIM/NPU 的宏须用 `ascendc_compile_definitions`（非 `target_compile_definitions`）才进设备编译，否则 kernel 退回 `seed_d` 生产分支致 SIM 全错 |

### 4.3 首版 launch 草图

```text
aclInit / CreateStream
  │
  ├─ [可选] Launch-0: f203_kem_kg_marker
  │
  ├─ Launch-1: f203_kem_kg_pke          // vendor stable：UB 内 d→ρ/σ → prep + NTT + encode
  │            → ek_gm[1568], dk_pke_gm[1536]   // d/z 不落 GM
  │            aclrtSynchronizeStream
  │
  └─ Launch-2: f203_kem_kg_finish       // AIV 标量；UB 内已有/再生 z[32]
               H(ek) → h_gm[32] 或 UB→拼接缓冲
               concat → dk_kem_gm[3168], copy ek → ek_kem_gm
               // 全程不写 d.bin/z.bin；z 仅出现在 dk_kem 尾 32B
               aclrtSynchronizeStream
  │
  D2H → output/ek_kem.bin, dk_kem.bin
aclFinalize
```

若 SIM 证明 Launch-1 与 Launch-2 可合并为**单 kernel 尾段**（PKE 末 sync 后直接接 H/z/concat），可减为 **1×PKE + 内嵌尾**；合并前须 G1 分段 PASS。

---

## 5. 目录结构（实现阶段）

```text
fix-f203-alg19-kem-keygen-correctness-k4/
├── INTEGRATION_PLAN.md
├── STATUS.md
├── SELF_CONTAINED.md
├── CMakeLists.txt
├── run.sh
├── main_kem_keygen.cpp
├── vendor/
│   └── pke_keygen/          # 自 stable 同步复制（vendor_sync 脚本）
├── kem/
│   ├── f203_kem_kg_derand_ub.hpp       // DerandZFromSeedD 等；d/z UB 驻留
│   ├── f203_kem_kg_finish.hpp
│   └── f203_kem_kg_finish_entry.cpp
├── scripts/
│   ├── gen_data.py
│   └── host_golden/
│       ├── golden_ek_dk.py
│       └── gate_g1_pke.py …
└── cmake/
```

**vendor 同步**：实现时新增 `scripts/vendor_sync_from_stable_keygen.sh`，从 stable 复制清单（与 Encrypt 探针 `vendor_sync` 同治理）；每次 stable 修订后手动或 CI 提示再同步。

---

## 6. 验收命令（目标）

```bash
# 探针内（host golden）
cd ascendc-tests/fix-f203-alg19-kem-keygen-correctness-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# L2 liboqs（仓库根）
bash scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash scripts/liboqs_kem_vs_ascendc.sh -r sim -v Ascend910B4
```

---

## 7. 与后续探针的边界

| 探针 | 算法 | 关系 |
|------|------|------|
| **本目录** | Alg.19 KeyGen | `ek_kem` / `dk_kem` |
| [`fix-f203-alg20-kem-encaps-correctness-k4`](../fix-f203-alg20-kem-encaps-correctness-k4/) | Alg.20 correctness | 消费 alg19 `ek_kem`；vendor frozen Encrypt |
| [`pass-fix-f203-alg20-kem-encaps-device-k4`](../pass-fix-f203-alg20-kem-encaps-device-k4/) | Alg.20 device | **T19a**；无 vendor |
| [`fix-f203-alg21-kem-decaps-correctness-k4`](../fix-f203-alg21-kem-decaps-correctness-k4/) | Alg.21 correctness | 消费 `dk_kem`+`c`；vendor frozen |
| [`pass-fix-f203-alg21-kem-decaps-device-k4`](../pass-fix-f203-alg21-kem-decaps-device-k4/) | Alg.21 device | **T19b/c**；无 vendor |

**不在 `examples/incubating/exp-*` 首版**；KEM 全阶段均在 `ascendc-tests/`。

---

## 8. 实现顺序（家里 Agent 建议）

1. `scripts/vendor_sync_from_stable_keygen.sh` + 最小 `CMakeLists` / `run.sh` 壳（G0）。
2. vendor PKE 段单跑 G1，与 stable 同 `seed_d` 对拍 `ek_pke`/`dk_pke`。
3. 设备 `H(ek)` G2；**UB 内 `DerandZFromSeedD`**（对齐 liboqs `d‖z` fixture）。
4. G3 拼接 + `KEM_KEYGEN_VERIFY=1` CPU+SIM。
5. 仓库 `scripts/liboqs_kem_vs_ascendc.sh`（待建）L2 对拍（64B seed 仅 fixture 侧，不进探针生产路径）。
6. 刷新 `STATUS.md`、[`docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md`](../../docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md)、当日 `qa/`。

---

## 9. 风险与阻塞

| 风险 | 缓解 |
|------|------|
| vendor PKE 与 stable 漂移 | `vendor_sync` 清单 + G1 回归 |
| SIM 多 launch / func_key | 优先 2 launch；必要时合并尾段 |
| `z` 与 liboqs derand 不一致 | 先 `liboqs_kem_fixture` dump **64B `d‖z`**，再实现 `DerandZFromSeedD` |
| 误导出 `d`/`z` | `SELF_CONTAINED` 审查 + 禁 `DEBUG_DUMP` 写 secret；生产 `run.sh` 仅 `ek_kem`/`dk_kem` |

---

## 10. 参考

| 文档 | 链接 |
|------|------|
| PKE liboqs 交叉验证 | [`docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](../../docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md) |
| stable KeyGen | [`examples/stable/stable-fips203-mlkem-pke-keygen-k4/`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/) |
| 设备 SHA3 | [`library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`](../../library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp) |
| SIM 约束 | [`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md) |
| 讨论纪要 | [`qa/2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md`](../../qa/2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md) |
