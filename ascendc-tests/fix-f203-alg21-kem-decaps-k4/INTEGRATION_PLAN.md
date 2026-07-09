# INTEGRATION_PLAN — fix-f203-alg21-kem-decaps-k4

**定位**：`ascendc-tests/` **Alg.21 `ML-KEM.Decaps(dk, c)` 设备全链正确性探针**（**ml_kem_1024 / k=4**）。对外 Algorithm 21；内部 **Algorithm 18 `Decaps_internal`** = PKE Decrypt + FO（`G`、重加密比对、隐式拒绝 `J`）。

**自包含**：[`SELF_CONTAINED.md`](SELF_CONTAINED.md) · [`用例自包含与设备全链约束.md`](../../docs/engineering/用例自包含与设备全链约束.md)。

**为何最重**：单条生产路径在 device 上串联 **完整 PKE Decrypt（~427k SIM tick）** + **完整 PKE Encrypt G5（~1030k tick）** + KEM FO 尾段；**不是**「又写一套 Encrypt/Decrypt」，而是 **vendor 两段已验收链 + 嵌入式 KEM 钩子**。

---

## 1. 目标与不变量

### 1.1 FIPS 数据流（liboqs I/O 锁定）

**`dk_kem` 3168B 布局**（与 alg19 / liboqs 一致）：

```text
dk_kke[0:1536]    = dk_pke   (ByteEncode₁₂(ŝ))
dk_kem[1536:3104] = ek       (= ek_kem)
dk_kem[3104:3136] = h        (= H(ek))，Decaps 直接读，不必重算
dk_kem[3136:3168] = z        (隐式拒绝秘密)
```

**Algorithm 18 `Decaps_internal(dk, c)`**（设备全链须覆盖）：

```text
m'  ← K-PKE.Decrypt(dk_pke, c)              // Alg.15，vendor
(K', r') ← G(m' ‖ h)                        // SHA3-512；K'=Kr[0:32], r'=Kr[32:64]
c'  ← K-PKE.Encrypt(ek, m', r')             // Alg.14 G5，vendor
K   ← K'                         if c = c'
    ← J(z ‖ c)                   otherwise   // SHAKE256；常数时间选择
```

| 不变量 | 说明 |
|--------|------|
| **设备全链** | 默认 `run.sh`：`dk_kem`+`c` → device 全链 → **仅** `output/K.bin` |
| **输入来源** | `gen_data` **复制** alg19 `dk_kem.bin` + alg20 `c.bin`（`DK_KEM_SRC`/`C_SRC`）；**不**内嵌 KeyGen/Encaps launch |
| **拼装** | vendor **alg15 G4** + **alg14 G5**（源见下）+ `kem/` FO 钩子；禁止跨探针 `#include` |
| **单 session** | **目标**：一次 `aclInit` / 一个 `stream`（对齐 alg15/20）。**首版 SIM 例外**：见 §11（两段 session workaround） |
| **func_key** | 合并 binary 后 **AIV-only 仍须 ≤5**（§4.3 核心设计约束） |
| **中间态** | `m'`/`c'`/`K'` **仅 GM/UB**；**禁止**生产路径落盘（`KEM_DECAPS_VERIFY=1` gate 脚本可读 GM dump env，非默认） |

### 1.2 生产 I/O（锁定）

| 文件 | 尺寸 | 来源 |
|------|------|------|
| `input/dk_kem.bin` | 3168 | alg19 `output/`（`DK_KEM_SRC`） |
| `input/c.bin` | 1568 | alg20 `output/`（`C_SRC`） |
| `input/lut_*.bin` | 同 alg14/15 | `host_golden/ntt_lut_bins.py` |
| `output/K.bin` | 32 | 设备 FO 输出 |

环境变量（锁定）：

| 变量 | 默认 |
|------|------|
| `SEED_D` | `20260619`（三探针一致） |
| `DK_KEM_SRC` | `../fix-f203-alg19-kem-keygen-k4/output/dk_kem.bin` |
| `C_SRC` | `../fix-f203-alg20-kem-encaps-k4/output/c.bin` |

---

## 2. 已验收上游（vendor 清单）

| Decaps 段 | FIPS | vendor 源 | 本目录 |
|-----------|------|-----------|--------|
| **D0** unpack + decode ŝ | Alg.15 | frozen alg15 G4 `g4_prep` | `vendor/pke_decrypt/` |
| **D1** NTT(u)+su_dot+INTT→m' | Alg.15 | frozen alg15 `chain_ntt`/`chain_intt` | 同上 |
| **K0** 解析 dk 切片 | Alg.18 | **新** `kem/f203_kem_dec_layout.hpp` | 标量/GMM 偏移常量 |
| **K1** `G(m'‖h)` | Alg.18 | `fips203_device_sha3.hpp` | **嵌入** §4.3 |
| **E0** Encrypt G5 全链 | Alg.14 | frozen alg14 G5（含 `Compress_5 (1<<26)`） | `vendor/pke_encrypt/` |
| **K2** `c` vs `c'` + `J(z‖c)` + 选 K | Alg.18 | **新** `kem/f203_kem_dec_fo.hpp` | **嵌入** §4.3 |

**vendor 同步源（锁定）**：

| 树 | `vendor_sync` 源 | 为何不是 stable |
|----|------------------|-----------------|
| Encrypt G5 | [`frozen-fix-f203-alg14-pke-encrypt-correctness-k4`](../frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/) | stable Encrypt **无** `pack/` / `main_encrypt_g5_run` |
| Decrypt G4 | [`frozen-fix-f203-alg15-pke-decrypt-correctness-k4`](../frozen/frozen-fix-f203-alg15-pke-decrypt-correctness-k4/) | stable Decrypt 为 **1-kernel fused**；本探针 Phase-D 仍 **2-launch** |

**禁止**：从 alg20 复制 `kem_enc_prep_re`（Encaps 采 `m`）；Decaps 的 `m'` 来自 Decrypt，`coins` 来自 `G(m'‖h)` 后半。

---

## 3. 核心设计决策（必读）

### 3.1 为何不能「子进程先 Encaps 再 Decrypt」

违反 [`SELF_CONTAINED.md`](SELF_CONTAINED.md) 与设备全链；且无法验收 **FO 常数时间路径** 在 device 上完成。

### 3.2 为何不能「再加 2～3 个独立 AIV KEM 核」

alg14 G5 已占 **5 个 AIV-only** `func_key` 名额（含 marker 时顶满）。Decrypt 三段为 **MIX**，不占 AIV。

若再 **独立** `kem_dec_g`、`kem_dec_fo` AIV 核 → 合并 `KERNEL_FILES` 后 **AIV ≥ 6** → SIM **507000**（见 [`AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md)）。

**首版策略（强制）**：**不新增 AIV launch 入口**；KEM 逻辑 **嵌入** 已有 MIX/AIV 核尾部：

| 钩子 | 嵌入位置 | 时机 |
|------|----------|------|
| **K1 `G(m'‖h)`** | `f203_decrypt_g4_chain_intt` 尾段（m' 已出） | Decrypt 最后一个 launch 内，写 `K'_gm`、`r'_gm` |
| **K2 FO 比对 + `J` + 写 `K`** | `f203_encrypt_pack` 尾段（c' 已出） | Encrypt 最后一个 launch 内，读 `c_gm`（输入密文）、`z` from dk |

**生产 `KERNEL_FILES` 去掉 `marker_custom`**（G5 不调）；AIV 仍为 4（prep_a_hat、prep_re、at_r5、g4_noise）+ 0 新增 = **≤5**。

> 若嵌入后 `nm device_aiv.o` 仍 >5：将 `g4_noise` 改 MIX 占位（alg14 备选方案），为 FO 扩展腾出 key — **以实测为准**。

### 3.3 为何仍会有 ~12+ 次 `aclrtLaunchKernel`

| 阶段 | launch 次数 | 说明 |
|------|-------------|------|
| Decrypt | **2** host 组（prep \| ntt+intt） | 与 alg15 相同，**不可并成 1**（SIM û 错） |
| Re-Encrypt | **9** | 与 alg20/alg14 G5 相同 |
| **合计** | **~11** | 算法链决定；**不是** func_key 问题 |

**减少的是 AIV 核种类与错误 launch 拓扑**，不是把 Decrypt+Encrypt 并成 KeyGen 式 3 launch。

### 3.4 编译体量（WSL 友好）

| 项 | 预估 |
|----|------|
| `KERNEL_FILES` | decrypt **3** + encrypt **9**（无 marker）= **12** 编译单元 |
| `build/` | **~120–150MB**（较 alg20 ~89MB 更大） |
| bisheng | 首编 **~8–12min**（`CMAKE_BUILD_JOBS=2`） |

**run.sh 自 day1**：

- `KEM_DECAPS_SKIP_REBUILD=1` **默认开**（有 stamp + 二进制则跳过）
- `KEM_DECAPS_FORCE_REBUILD=1` 才清 `build/`
- `CMAKE_BUILD_JOBS=2`
- `KERNEL_COMPUTE_BUDGET_SEC=1800`（SIM ~15min+ 缓冲；仅计算段）

---

## 4. Launch 编排（首版）

### 4.1 单 session 总览

```text
aclInit / CreateStream
  H2D: dk_kem, c, LUT
  │
  ├─ Phase-D（Alg.15，同 alg15 G4）
  │    Launch-D1: f203_decrypt_g4_prep
  │      unpack(c→u,v) + decode(dk_pke→ŝ)
  │    aclrtSynchronizeStream
  │    Launch-D2a: f203_decrypt_g4_chain_ntt
  │    sync
  │    Launch-D2b: f203_decrypt_g4_chain_intt
  │      … → m'_gm[32]
  │      ★ K1: G(m'‖h) → K'_gm, r'_gm   （intt 核尾嵌入）
  │    sync
  │
  ├─ Phase-E（Alg.14 G5，同 alg20/alg14）
  │    prep_a_hat(ρ from ek)
  │    prep_re(coins=r', m=m')          // 无 DerandM
  │    ntt_r → decode_t_hat → sync
  │    host 拼 matM → at_r5
  │    intt×2 → g4_noise → pack
  │      … → c'_gm inside pack
  │      ★ K2: ct_compare(c, c'); J(z‖c); select → K_gm
  │    sync
  │
  D2H: K.bin
aclFinalize
```

**`h`、`z`、`ek`**：host 在 H2D 前从 `dk_kem` 切片拷入 GM（**布局解析非密码学**）；或 device 在 D1 prep 从 `dk_kem_gm` 标量偏移读取 — 实现时二选一，**须在 G1 gate 锁定**。

### 4.2 Re-Encrypt 与 Encaps 差异

| 项 | Encaps (alg20) | Decaps (alg21) |
|----|----------------|----------------|
| `m` 来源 | `DerandMFromSeedD(seed_d)` | **`m'` from Decrypt** |
| `coins`/`r` | `G(m‖H(ek))` 后半 | **`G(m'‖h)` 后半** |
| `prep_re` 入口 | `kem_enc_prep_re` | **vendor `prep_re`**（仅改 GM 输入源） |
| `H(ek)` | 设备算 | **从 dk 读 `h`**（K1 用 `m'‖h`） |
| 输出 | `c`+`K` | **仅 `K`**（`c` 为输入） |

### 4.3 func_key 验收门禁（实现后第一件事）

```bash
nm build/ascendc_kernels_sim_aiv_device_dir/device_aiv.o | grep ' T f203'
# 生产 build：≤5 行 AIV-only；要 launch 的核 func_key ≤4
```

失败时按优先级释 key：`marker` 不编入 → `g4_noise` MIX 化 → 收紧 FO 嵌入（禁止独立 FO 核）。

---

## 5. 分阶段 Gate

| Gate | 路径 | 验收 |
|------|------|------|
| **G0** | vendor_sync + cmake 壳 | 结束正常 |
| **G1** | Phase-D only | `m'` vs alg15 同 `dk_pke,c` max=0 |
| **G2** | G1 + K1 | `K'`,`r'` vs host_golden |
| **G3** | Phase-E alone（fixture `ek,m',r'`） | `c'` vs alg14 max=0 |
| **G4** | D+E+K2 全链 | `K` vs `golden_K` max=0 |
| **G5** | 生产 I/O | 默认 `run.sh`；仅 `K.bin` |

**过渡**：G4 CPU+SIM PASS 后冻结 G0–G3 分段 env。

### 5.1 标准测试向量（锁定）

```text
SEED_D=20260619
1. alg19 → dk_kem.bin, ek_kem.bin
2. alg20 → c.bin, K_enc.bin（Encaps 共享秘密，仅 golden 用）
3. alg21 → K.bin  应对 Encaps 的 K（合法 c 路径）
```

**拒绝路径**（`KEM_DECAPS_VERIFY=2` 可选）：篡改 `c` 一字节 → `K` 应等于 `J(z‖c)` 而非 `K_enc`（host golden 或 liboqs）。

---

## 6. 目录结构

```text
fix-f203-alg21-kem-decaps-k4/
├── INTEGRATION_PLAN.md
├── STATUS.md
├── SELF_CONTAINED.md
├── run.sh
├── main_kem_decaps.cpp
├── main_kem_dec_g5_run.cpp          # 编排 D+E 单 session
├── f203_kem_dec_layout.h
├── vendor/
│   ├── pke_decrypt/                 # vendor_sync_from_alg15_decrypt.sh
│   └── pke_encrypt/                 # vendor_sync_from_alg14_encrypt.sh
├── kem/
│   ├── f203_kem_dec_g.hpp           # G(m'‖h) 嵌入 intt 尾
│   ├── f203_kem_dec_fo.hpp          # ct_compare + J + select 嵌入 pack 尾
│   └── patches/                     # 对 vendor 文件的极小 diff 说明（或 patched 副本）
├── scripts/
│   ├── gen_data.py                  # 复制 dk_kem + c + LUT
│   ├── vendor_sync_from_alg15_decrypt.sh
│   ├── vendor_sync_from_alg14_encrypt.sh
│   ├── verify_kem_decaps.py
│   └── host_golden/
│       ├── golden_K.py
│       ├── gate_g1_m_prime.py
│       └── gate_g3_reencrypt.py
└── cmake/decaps/CMakeLists.txt
```

**vendor 补丁策略**：优先 **不改 vendor 源**，在 `kem/*_patch.hpp` 用 `__attribute__((weak))` 或 **fork 最小 entry 包装**；若必须改 `chain_intt`/`pack` entry，在 `vendor_sync` 后 `scripts/apply_kem_patches.sh` 打补丁并 **文档化 diff**（便于 alg14/15 更新后重放）。

---

## 7. 验收命令

```bash
# 前置（同 SEED_D，各跑一次即可；后续 SKIP_REBUILD）
cd ascendc-tests/fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg20-kem-encaps-k4
KEM_ENCAPS_SKIP_REBUILD=1 KEM_ENCAPS_VERIFY=1 bash run.sh -r cpu -v Ascend910B4

# Decaps
cd ../fix-f203-alg21-kem-decaps-k4
KEM_DECAPS_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
KEM_DECAPS_SKIP_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim -v Ascend910B4

# L2（实现后扩 scripts/liboqs_kem_vs_ascendc.sh decaps 段）
```

---

## 8. 实现顺序（建议）

| 步 | 内容 | 阻塞 |
|----|------|------|
| 1 | `vendor_sync` ×2 + `run.sh` 壳 + `gen_data` 复制 I/O | — |
| 2 | **G1**：仅 Phase-D，对标 alg15 | — |
| 3 | **K1 嵌入** intt 尾 + G2 gate | G1 PASS |
| 4 | **G3**：仅 Phase-E（fixture m',r'） | vendor encrypt |
| 5 | **K2 嵌入** pack 尾 + G4 全链 CPU | G2+G3 PASS |
| 6 | `nm` func_key 审计 + SIM | CPU PASS |
| 7 | liboqs decaps 段 + STATUS/qa/note | **G4 CPU+SIM PASS**（SIM 为 workaround，§11） |

---

## 9. 风险

| 风险 | 缓解 |
|------|------|
| func_key >5 | §4.3 嵌入策略；禁止独立 FO AIV 核 |
| SIM 超时 / WSL OOM | SKIP_REBUILD 默认；SIM 单独跑；勿叠 alg19/20 重编 |
| intt/pack 补丁与上游 vendor 漂移 | `apply_kem_patches.sh` + gate 回归 alg14/15 |
| `h` 从 dk 读 vs 重算 `H(ek)` | 与 liboqs 布局一致即可；golden 用 liboqs decaps |
| 常数时间 FO | 首版语义正确优先；`Select` 用 AscendC 逐 lane 掩码；note 记后续 hardening |
| **SIM 单 session D+E 污染** | §11；当前两段 session + host FO 比对；**非终态** |

---

## 10. 参考

| 文档 | 链接 |
|------|------|
| alg15 2-launch | [`docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md`](../../docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md) |
| alg20 Encaps | [`../fix-f203-alg20-kem-encaps-k4/INTEGRATION_PLAN.md`](../fix-f203-alg20-kem-encaps-k4/INTEGRATION_PLAN.md) |
| func_key | [`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md) |
| qa §7 | [`qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md`](../../qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md) §7 |
| note | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |
| 首版 STATUS | [`STATUS.md`](STATUS.md) |

---

## 11. 首版实现记录与 SIM 问题（2026-07-02）

> 本节记录**相对 §3–§4 规划的偏差**、**已确认根因**与**遗留**；细节见 [`STATUS.md`](STATUS.md)。

### 11.1 相对规划的架构变更

| 项 | §3/§4 规划 | 首版落地 | 原因 |
|----|------------|----------|------|
| K1 `G(m'‖h)` | 嵌入 `f203_decrypt_g4_chain_intt` 尾 | **独立 AIV** `f203_kem_dec_g` | intt 尾嵌入调试周期长；独立 launch 便于 G2 分段 dump |
| K2 FO | 嵌入 `f203_encrypt_pack` 尾 | **独立 entry** `f203_kem_dec_pack_entry.cpp`（CPU 设备 FO） | 与 alg14 原 pack 并存于 encrypt kernel |
| decrypt+encrypt | 单 `KERNEL_FILES` / 单 `.so` | **分库** `ascendc_kernels_*_decrypt` / `_encrypt` | 合并编译 **`tiling` 头重定义** |
| K1 与 intt | 同一 launch | **intt 后 sync + 独立 G launch** | 与上表独立核一致 |
| SIM Phase-E | 单 session 接续 §4.1 | **session-2** `run_g5_sim_full` | §11.2 CAModel 污染 |

**func_key**：encrypt 侧同时编入 `f203_encrypt_pack_entry.cpp`（fresh session 用）与 `f203_kem_dec_pack_entry.cpp`（CPU 单 session 用）；**待 `nm` 审计**。

### 11.2 SIM 单 session 失败（已确认）

**现象**（`KEM_DECAPS_VERIFY=1`，单 session 编排）：

```text
Phase-D + K1：m', K', coins  dump max=0  ✓
Phase-E：c' vs c  max=244  ✗
FO：K vs golden  max=216  ✗（走 J(z‖c) 拒绝路径）
```

**已排除**：

- `SEED_D` / 输入复制错误（固定 `20260619`，与 alg19/20 一致）
- `m'`、`G(m'‖h)` 输出错误（SIM dump **max=0**）
- Encrypt 算法本身错误（**同** `m'/coins` 单独 alg14 G5 SIM → **max=0**）

**仍 FAIL 的尝试**：

- 单 session 内释放 decrypt 侧 GM、重载 encrypt LUT → **c' 仍错**

**当前结论**：CAModel/ACL **超长单 session** 内 Decrypt 完成后立即跑 Encrypt G5 时，**Phase-E 输出被污染**；污染机制未定位。

**workaround（当前 SIM PASS 路径）**：

```text
session-1: aclInit → Phase-D → f203_kem_dec_g → sync → aclFinalize
session-2: run_g5_sim_full(ek, coins, m) → c'
host: memcmp(c, c') ? fail : write K'
```

**局限**：

- 不符合 §1.1「单 session」不变量（SIM）
- SIM **未执行**设备 FO（无 `J(z‖c)` 拒绝分支验证）
- CPU 仍为 §4.1 单 session + 设备 FO → **G4 PASS**

### 11.3 构建踩坑（实现期）

| 问题 | 处理 |
|------|------|
| decrypt+encrypt 单 lib → agents `tiling` 重定义 | **分库** |
| SIM precompile 缺 AscendC include | `cmake/decaps` 加 `ascendc_include_directories` |
| 手写 `chain_intt` `_do` wrapper | 与 auto_gen 冲突 → **删除**，用 vendor entry + 独立 G |
| `vendor_sync` 缺 alg14 host 编排 | 同步 `main_encrypt_g5_run.cpp`、`f203_encrypt_g5_run.hpp` |
| CPU 链 alg14 host 与 decaps pack 符号冲突 | CPU **不**链 `main_encrypt_g5_run`；SIM 才链 |
| fresh session 缺原 pack 符号 | encrypt kernel **同时**含 `f203_encrypt_pack_entry.cpp` |

### 11.4 验收状态（2026-07-02）

| Gate | CPU | SIM |
|------|-----|-----|
| G4 合法 c → K | **PASS** max=0 | **PASS** max=0（workaround） |
| G5 生产 I/O | **PASS** | **PASS** |
| 拒绝路径 | 未测 | **未测** |
| func_key | 待 `nm` | 待 `nm` |

### 11.5 后续（P0）

1. 定位并修复 SIM 单 session Phase-E 污染，恢复 §4.1 编排。
2. SIM 拒绝路径：`KEM_DECAPS_VERIFY=2` 或设备 FO 单 session 验证。
3. `nm` func_key 分库审计；`liboqs` decaps 段。
