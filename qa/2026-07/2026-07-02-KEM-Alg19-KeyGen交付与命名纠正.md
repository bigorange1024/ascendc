# 2026-07-02 KEM Alg.19 KeyGen 交付收尾、目录命名纠正与「为何顺利」

关键词：**Alg.19 KeyGen** · **fix-f203-alg19-kem-keygen-correctness-k4** · **T6 关闭** · **alg16→alg19 重命名** · **vendor PKE** · **顺利原因**

---

## 1. 当日进展

### 1.1 交付状态（延续 7/1 实现，本日收尾）

| 项 | 结论 |
|----|------|
| **探针** | [`../../ascendc-tests/fix-f203-alg19-kem-keygen-correctness-k4/`](../../ascendc-tests/fix-f203-alg19-kem-keygen-correctness-k4/) |
| **标准定位** | 对外 **FIPS 203 Alg.19** `ML-KEM.KeyGen()`；内部经 **Alg.16 `KeyGen_internal(d,z)`** 拼装 |
| **架构** | 3 launch：`prep`（vendor PKE + `d`）\| `mmad`（Alg.13 计算 + `ek_pke`）\| `kem_finish`（`H(ek)` + UB `z` + `dk_kem` 拼接） |
| **I/O** | `ek_kem` 1568B · `dk_kem` 3168B（liboqs：`dk_pke‖ek‖H(ek)‖z`）· `SEED_D=20260619` |
| **验收** | CPU+SIM **`KEM_KEYGEN_VERIFY=1` max=0**；`scripts/liboqs_kem_vs_ascendc.sh` CPU+SIM **max=0**；SIM tick **742558**；无 507000 |
| **TODO** | **T6** → **PASS / 关闭** |

自研 KEM 增量核仅 `kem/` 下 3 个文件（`derand_ub` + `finish` + `finish_entry`），其余为 vendor 复制的 stable PKE 全链。

### 1.2 目录与文档命名纠正（用户拍板）

用户明确：实现对象是 **Alg.19**，不是 Alg.16（Alg.16 仅为 internal 步骤名）。

| 变更 | 说明 |
|------|------|
| `fix-f203-alg16-kem-keygen-k4` → **`fix-f203-alg19-kem-keygen-correctness-k4`** | 探针目录 |
| `F203-KEM-Alg16-KeyGen…` → **`F203-KEM-Alg19-KeyGen…`** | `docs/notes/` 技术总结 |
| qa 纪要文件名 **Alg16** → **Alg19** | 7/1 纪要保留内容，7/2 本篇接续 |
| `prep` 符号链接 | 重命名后曾指向旧 `alg16` 路径 → 已改为 `vendor/pke_keygen/prep` |
| `AGENT_HANDOFF` / `qa/TODO` / 各 `INDEX` | 探针标 **Alg.19 PASS**；下一任务指向 Alg.17/18 |

重命名后 **CPU 冒烟复验 PASS**（`rm -rf build out` 全量重编）。

---

## 2. 为什么 KEM 实现这么顺利？

**结论先行**：顺利不是因为 KEM 本身「简单到可以糊弄」，而是因为 **难的部分已在 PKE stable 里付过学费**；本探针在密码学上只是 **薄增量层**，在工程上 **严格复用已验证拼装模式**。

### 2.1 密码学工作量高度前置（PKE 占 95%+）

| 段 | FIPS | 本探针工作量 |
|----|------|--------------|
| NTT / Â / s,ê / ek,dk_pke | Alg.7–13 | **vendor 整段复制** stable，已 liboqs max=0 |
| `H(ek)` | Hash | 单次 **SHA3-256(1568B)**，API 已在 KeyGen/Alg.7 用过 |
| 采 `z` | Alg.19 行 2 | **32B** 域分离 SHA3，与 prep 内 `DerandFromSeedD` **同型** |
| 拼 `dk_kem` | Alg.16 行 4 | GM 标量 `memcpy` 式四段拼接，无新算子 |

相对 Encrypt（basemul + compress + 多段 UB）或 Decrypt（INVNTT + 解压），KEM KeyGen **不引入新多项式算子、无 MIX 跨核新契约**。

### 2.2 工程模式已成熟（7 月前 PKE 三件套铺好路）

1. **单 ACL session、多 launch 编排** — Encrypt G5 / Decrypt G4 / round-trip 已验证；KEM 沿用 **3 launch、禁止子进程调 stable `run.sh`**。
2. **507000 / func_key** — 6/30 已定性「≥5 次 func_key 必炸」；本探针 launch 数与 KeyGen stable 同量级，**未踩新雷**。
3. **设备 SHA3 分层** — `fips203_device_sha3.hpp` 稳定 API；KEM 只调用 `Sha3OneShot`，未新开 SHAKE 向量路径。
4. **liboqs 交叉验证流水线** — PKE 三阶段脚本跑通后，KEM 仅需 `liboqs_kem_ref.c` + fixture 锁 `d‖z` 域分离，**L2 对拍即插即用**。
5. **vendor 自包含治理** — `vendor_sync_from_stable_keygen.sh` 一键同步；G1 隐含保证 `ek_pke/dk_pke` 与 stable 一致，KEM 尾段只关心 **拼接是否正确**。

### 2.3 需求边界清晰、无「方案摇摆」

7/1 用户一次性锁定：

- **Alg.19**：`d`/`z` **device UB**，禁止导出本体；
- **I/O**：liboqs **3168B** dk 布局（非 FIPS 最小 1600B 形态）；
- **范围**：不含 Encaps/Decaps；PKE 来自 stable vendor，不抄 frozen。

无 limbsplit / sepair / 双 session 等历史歧路 → **实现参数零摇摆**，INTEGRATION_PLAN 可直接落码。

### 2.4 自研增量极小，排错面窄

| 新增自研 | 行级规模 | 风险 |
|----------|----------|------|
| `DerandZFromSeedD` | ~75 行 | 与 `DerandFromSeedD` 对称；SIM 上 `const char[]` 改逐字符赋值（已知坑） |
| `KemKgFinishImpl` | ~40 行 | 纯标量 GM 搬运 + 两次 SHA3 |
| `main_kem_keygen` 编排 | 在 vendor main 上 +1 launch | CMake include 路径、`prep` 软链 |

7/1 实现当晚即 G3 PASS；7/2 重命名仅文档 + 软链，**未改密码学路径**。

### 2.5 与「不顺利」路线的对照（为何不能类推）

| 曾阻塞数周的主题 | KEM KeyGen 为何不同 |
|------------------|---------------------|
| NTT 向量 / Gather 禁令 / poly-batch | KEM **不重写 NTT** |
| Encrypt fake-Â / 布局审计 | KEM **无 basemul / compress** |
| func_key 爆炸 | launch  profile 与已 PASS 的 KeyGen 同类 |
| liboqs Compress_5 偏置 | KEM **无 compress** |

**教训**：下一项 **Alg.17/18 Encaps/Decaps** 会重新遇到 PKE 侧算子与随机性编排，**不能**假设与 KeyGen 同速；顺利是 **scope 切分正确** 的结果，不是 AscendC 突然变简单。

---

## 3. 遗留与下一任务

| ID | 事项 | 状态 |
|----|------|------|
| **T6** | Alg.19 KEM KeyGen | **PASS** |
| **T2** | Alg.17/18 Encaps/Decaps | **待建独立探针**（前置：PKE Enc/Dec + 本探针均已 PASS） |
| **T14a/T15a** | PKE Enc/Dec → stable 晋级 | 探针 PASS，stable 未建 |

索引：[`STATUS.md`](../../ascendc-tests/fix-f203-alg19-kem-keygen-correctness-k4/STATUS.md) · [`INTEGRATION_PLAN.md`](../../ascendc-tests/fix-f203-alg19-kem-keygen-correctness-k4/INTEGRATION_PLAN.md) · [`docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md`](../../docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md) · 前日 [`2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md`](2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md)。

---

## 4. Alg.20 Encaps 预研（参照 Alg.19 拼装范式）

**用户拍板（2026-07-02）**：在 Alg.19 PASS 后，下一 KEM 对外算子为 **FIPS 203 Algorithm 20** `ML-KEM.Encaps(ek)`（**非** Alg.17 单独探针名；Alg.17 为 internal）。

### 4.1 代数分解（与 Alg.19 对称）

```text
Alg.20 ML-KEM.Encaps(ek)
  m ←$ B^32                          // 行 1：device UB（对标 Alg.19 的 d/z）
  (K, c) ← ML-KEM.Encaps_internal(ek, m)   // Alg.17

Alg.17 Encaps_internal(ek, m)
  (K, r) ← G(m ‖ H(ek))              // G = SHA3-512 → 64B：K‖coins
  c ← K-PKE.Encrypt(ek, m, r)        // Alg.14 全链
  return (K, c)
```

| 对比 | Alg.19 KeyGen | Alg.20 Encaps |
|------|---------------|---------------|
| **对外随机性** | `d`, `z`（32B×2） | **`m`**（32B） |
| **重计算段** | vendor **Alg.13** KeyGen | vendor **Alg.14** Encrypt G5 |
| **薄 KEM 层** | `H(ek)` + 采 `z` + 拼 `dk_kem` | `H(ek)` + **`G(m‖H(ek))`** + 输出 `K` |
| **liboqs I/O** | `ek_kem` 1568 · `dk_kem` 3168 | `ek` 1568 · **`c` 1568** · **`K` 32** |
| **验收脚本** | `liboqs_kem_vs_ascendc.sh` keygen 段 | 待扩 **encaps** 段（`encaps_derand` / 黑盒 `encaps`） |

### 4.2 建议探针形态（镜像 `fix-f203-alg19-kem-keygen-correctness-k4`）

| 项 | 建议 |
|----|------|
| **目录** | `../../ascendc-tests/fix-f203-alg20-kem-encaps-correctness-k4/` |
| **vendor 源** | [`stable-fips203-mlkem-pke-encrypt-k4`](../stable-fips203-mlkem-pke-encrypt-k4/) G5 全链（`vendor_sync.sh` 复制，禁止 `#include` 跨探针） |
| **自研 KEM 头** | `kem/f203_kem_enc_init.hpp`：`DerandMFromSeed*`（可复现）+ `H(ek)` + `G(m‖h)` 拆 `K`/`r` |
| **编排** | 单 ACL session；**优先**将 `m`/`G`/`coins` 写入 GM 后接 vendor Encrypt launch 序列（避免额外 func_key） |
| **生产 I/O** | `input/ek_kem.bin` 1568B；Host **不**提供 `m`/`coins`；`output/c.bin` + `output/K.bin` |
| **Golden** | `KEM_ENCAPS_VERIFY=1` + 仓库级 `liboqs_kem_encaps` 对拍（仿 keygen 脚本） |

### 4.3 Gate 建议

| Gate | 内容 | 验收 |
|------|------|------|
| G0 | launch 壳 + vendor_sync | kernel 结束 |
| G1 | vendor Encrypt  alone（fixture `m`/`coins`） | vs alg14 同输入 `c` max=0 |
| G2 | 设备 `H(ek)+G(m‖h)` | vs host golden 中间量 |
| G3 | Encaps_internal 全链 | `c`+`K` max=0 |
| G4 | Alg.20：`m` UB 采样 + internal | 默认 `run.sh` |

### 4.4 风险（**不会**像 Alg.19 一样「一夜 PASS」）

1. **算子重量**：Encrypt G5 SIM **~922k tick**、多 launch；KEM 头再轻也绑在同一 session 预算上。
2. **func_key ≤5**：alg14 已压到 5 个 AIV 核；新增独立 `kem_enc_init` 核可能触发 **507000** → 首选 **并入 `prep_re` 或 `prep_a_hat` 前段**（在写 `coins_gm` 之前完成 `m`+`G`）。
3. **`G` 与 `Encrypt` 数据面**：`r` 必须作为 `coins` 喂给 `f203_encrypt_prep_re`；`m` 同时作 Encrypt 明文 — UB→GM 布局须在 INTEGRATION_PLAN 锁定。
4. **Compress_5**：Encrypt 路径已修 `(1<<26)`；vendor 时必须带上 pack 修复版本。

### 4.5 与 Alg.21 Decaps 的衔接

Alg.21 为纯 **Alg.18 internal**（无新随机性）：`Decrypt` + `G` + 重加密比对 + implicit rejection `J(z‖c)`。可 vendor [`fix-f203-alg15`](../stable-fips203-mlkem-pke-decrypt-k4/) + 更厚的 KEM 尾段；**Encaps 探针应预留 `dk_kem` 3168B 布局解析**（`dk_pke‖ek‖H(ek)‖z`）供后续 Decaps 探针复用。

**TODO**：新开 **T7a** — Alg.20 探针 **规划**；写码前须用户确认目录 + customspec 不适用（`ascendc-tests` 探针）。

---

## 5. Alg.20 探针目录建立（用户拍板 · 2026-07-02 续）

| 项 | 结论 |
|----|------|
| **目录** | [`../../ascendc-tests/fix-f203-alg20-kem-encaps-correctness-k4/`](../../ascendc-tests/fix-f203-alg20-kem-encaps-correctness-k4/) |
| **流程** | **先方案后代码**；当前文档主线为 `INTEGRATION_PLAN` / `qa` / `docs/notes` |
| **公钥 `pk`** | **读 alg19 产出** `output/ek_kem.bin` → 本探针 `input/`（`gen_data` + `EK_KEM_SRC` 默认相对路径） |
| **随机性 `m`** | device UB（`DerandMFromSeedD`）；Host 仅 `seed_d` |
| **TODO** | **T7a** P0 规划中 |

---

## 6. Alg.20 Encaps 首版写码与生产 I/O 治理（2026-07-02 续 · 家里）

### 6.1 实现与验收

| 项 | 结论 |
|----|------|
| **代码** | vendor alg14 G5 + KEM 头并入 `f203_kem_enc_prep_re`（无第 6 AIV 核） |
| **pk** | **只复制** alg19 `output/ek_kem.bin` → `input/`；**禁止** alg20 `run.sh` 内嵌 KeyGen |
| **生产 output** | 仅 **`c.bin` + `K.bin`**；**禁止**落盘 `a_hat`/`r_hat` 等中间张量 |
| **CPU** | `KEM_ENCAPS_VERIFY=1 bash run.sh -r cpu` → **c/K max=0 PASS**（~51s，`CMAKE_BUILD_JOBS=2`） |
| **SIM** | 早前单 session **PASS** tick **1029406**；本轮改 run.sh 后 **待办公室复验** |
| **TODO** | **T7a** → **CPU PASS / SIM 待复验** |

### 6.2 `run.sh` 重写（alg20，已落地）

- `KEM_ENCAPS_SKIP_REBUILD=1`：RUN_MODE 未变且二进制在则跳过 cmake
- `KEM_ENCAPS_FORCE_REBUILD=1`：才 `rm -rf build out`
- 默认 `CMAKE_BUILD_JOBS=2`（WSL 勿 `-j` 满核）
- 缺 `EK_KEM_SRC` **直接 exit 2**，不拉 alg19

### 6.3 alg14 同步（代码已改 · **run.sh 待对齐**）

| 已做 | 待做（办公室 P0） |
|------|-------------------|
| G5 仅写 `c.bin`；去掉默认 `verify_gate` | `run.sh` 对齐 alg20：`ENCRYPT_SKIP_REBUILD` / `FORCE_REBUILD` / `CMAKE_BUILD_JOBS=2` |
| `main_encrypt_g5_run` SIM 去掉中间量 D2H | 去掉默认 `rm -rf build out`；保留 `rm -rf input output` → 改为 `mkdir -p` |
| `verify_gate.py` 标废弃（frozen G1–G4 手工回放） | CPU+SIM 各跑一轮 `verify_result.py` 回归 |

### 6.4 WSL 资源教训（Agent 勿再犯）

- **禁止**并行跑多个 SIM / 多个全量 `cmake -j` 编译
- alg20 SIM ~15min + 高内存；磁盘 100% 多因 **每次 rm -rf build + 满核 bisheng**
- 验收顺序：**先 CPU（SKIP_REBUILD）→ 单独 SIM**

### 6.5 办公室 Agent 接续清单

1. `git pull` → 读 [`AGENT_HANDOFF.md`](../../AGENT_HANDOFF.md)
2. **T7a**：`KEM_ENCAPS_SKIP_REBUILD=1 KEM_ENCAPS_VERIFY=1 bash run.sh -r sim`（勿并行其他 SIM）
3. **alg14 run.sh** 按 §6.3 对齐 + CPU/SIM 回归 `c.bin`
4. 扩 `scripts/liboqs_kem_vs_ascendc.sh` **encaps** 段（keygen 已有）
5. （可选）`docs/notes/` Alg.20 技术总结 · SIM tick 写入 STATUS

---

## 7. Alg.21 Decaps 首版写码与 SIM 单 session 问题（同日追加）

关键词：**Alg.21 Decaps** · **fix-f203-alg21-kem-decaps-correctness-k4** · **T7c** · **SIM CAModel 污染** · **两段 session workaround**

### 7.1 交付状态

| 项 | 结论 |
|----|------|
| **探针** | [`../../ascendc-tests/fix-f203-alg21-kem-decaps-correctness-k4/`](../../ascendc-tests/fix-f203-alg21-kem-decaps-correctness-k4/) |
| **架构** | vendor alg15 Decrypt G4 + alg14 Encrypt G5 + `kem/` K1 `G` + K2 FO |
| **I/O** | `dk_kem` 3168B + `c` 1568B → `K` 32B；输入复制 alg19/20（`SEED_D=20260619`） |
| **G4 CPU** | **PASS** `K max=0`（单 session 设备 FO） |
| **G4 SIM** | **PASS** `K max=0`（**两段 session** + host `memcmp(c,c')`） |
| **TODO** | **T7c** → **有条件 PASS**（SIM workaround；单 session / 拒绝路径待修） |

### 7.2 相对 INTEGRATION_PLAN 的偏差

- K1 **独立 AIV** `f203_kem_dec_g`（非 intt 尾嵌入）
- decrypt / encrypt **分库**（`tiling` 重定义）
- SIM：**非** §4.1 单 session；Phase-E 用 vendored `run_g5_sim_full` fresh session

### 7.3 SIM 根因（已确认）

**不是**种子或 `m'/coins` 错：

- dump `m'`、`K'`、`coins` **max=0**
- 同组输入单独 alg14 G5 SIM **max=0**

**是**单 session 内 Decrypt 后 Encrypt → **`c' max=244`** → FO 拒绝 → **`K max=216`**。

释放 GM / 重载 LUT **未修复**。fresh alg14 session **可规避**。

### 7.4 遗留

1. 深挖 CAModel 单 session 状态污染，恢复单 session + 设备 FO（SIM）
2. 拒绝路径 SIM（`KEM_DECAPS_VERIFY=2`）
3. `nm` func_key · liboqs decaps 段

文档：**老三样**已刷新（`INTEGRATION_PLAN.md` §11、本 qa §7、[`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md)）。

### 7.5 根因定位 + 单设备库合并（同日续 · 家里，公司拉库后接手）

关键词：**双设备库 func_key 冲突** · **单库合并** · **dec_aiv_func 改名** · **kem_dec_g 改 MIX** · **R3 触发面**

**根因修正**：§7.3 的 SIM `c' max=244` **不是**「泛化 CAModel 单 session 污染」。真因是探针曾用 **decrypt/encrypt 双设备 `.so`**：一个 ACL session 内两份 device binary **func_key 空间重叠 / 装载边界冲突**，decrypt 库先加载即「活跃」，encrypt 核 launch 被派发到错误 binary → `c'` 形状对值全错；fresh session 只 launch 一侧故恢复。本仓所有过关 SIM 探针皆单库单 session，decaps 唯一双库 = 差异变量。

**修法（用户拍板：单库合并；家里做重构+CPU，SIM 交公司）**：

| 改动 | 说明 |
|------|------|
| 合并单库 `ascendc_kernels_${RUN_MODE}` | `cmake/decaps/CMakeLists.txt` decrypt+encrypt+kem 同一 `ascendc_library` |
| `dec_aiv_func.hpp` 改名 | 双树 21 同名头逐个 `diff`：**仅 `aiv_func.hpp` 分歧**（NTT-forward vs NTT+INTT），单 `-I` 混路径裸名 include 拉错树 → CPU 编译报 `namespace tiling` 重定义。decrypt 改名 + 4 包含者改 include；`vendor_sync_from_alg15_decrypt.sh` sync 后幂等重放 |
| `kem_dec_g` AIV→MIX 占位 | 合库 AIV-only=5 触 R1（≥5→507000），改 MIX 回落 4 |
| main 默认单 session | 两段 session 降为 `KEM_DECAPS_SIM_2SESSION=1` 非默认回退 |

**证据（CPU）**：`KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r cpu` → `K max=0 PASS`；`out/lib/` 仅 `libascendc_kernels_cpu.so`。

**公司待验（SIM，家里 WSL 不跑重型 SIM）**：`SIM_DIRECT=1 KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim` → `K max=0`、`507000`=0、`dbg_c_prime==c`；`nm ...device_aiv.o` AIV-only ≤4；拒绝路径；liboqs decaps 段。

**知识库升级**：`AscendC-CAModel-SIM-funckey与单session约束知识库` 应补 **R3：一个 ACL session 内多个设备 `.so` → 无错误码但后段输出污染**（详见 Alg.21 note §4.3）。

**07-03 家里 SIM 实测（重要修正）**：`SIM_DIRECT=1 … run.sh -r sim` 实跑单库单 session：编译/链接 OK、**Phase-D（decrypt→G）走通**（`dbg_{m_prime,coins,K_prime}.bin` 产出），但 **Phase-E 重加密后一个 vector core 在 CAModel 无限自旋**（`sim_log/core0.veccore0.instr_log.dump` 单核 65MB+ 持续增长、255% CPU、~7min 不退，手动 kill）。→ 「双库 func_key 冲突」**不是唯一病根**；单库合并只消除了编译/链接层面的双库问题与 `max=244` 污染，**Phase-E 单 session 重加密链本身在 SIM 仍有死锁**（历史两段 session workaround 正为绕开此处）。公司排查聚焦 **Phase-E 首个自旋 kernel 的同步点/循环终止条件**（从 instr_log 末尾定位），或 `KEM_DECAPS_SIM_2SESSION=1` 两段回退保底。CPU 仍 `K max=0 PASS`，逻辑正确性不受影响。

