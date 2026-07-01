# 2026-07-02 KEM Alg.19 KeyGen 交付收尾、目录命名纠正与「为何顺利」

关键词：**Alg.19 KeyGen** · **fix-f203-alg19-kem-keygen-k4** · **T6 关闭** · **alg16→alg19 重命名** · **vendor PKE** · **顺利原因**

---

## 1. 当日进展

### 1.1 交付状态（延续 7/1 实现，本日收尾）

| 项 | 结论 |
|----|------|
| **探针** | [`ascendc-tests/fix-f203-alg19-kem-keygen-k4/`](../../ascendc-tests/fix-f203-alg19-kem-keygen-k4/) |
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
| `fix-f203-alg16-kem-keygen-k4` → **`fix-f203-alg19-kem-keygen-k4`** | 探针目录 |
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

索引：[`STATUS.md`](../../ascendc-tests/fix-f203-alg19-kem-keygen-k4/STATUS.md) · [`INTEGRATION_PLAN.md`](../../ascendc-tests/fix-f203-alg19-kem-keygen-k4/INTEGRATION_PLAN.md) · [`docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md`](../../docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md) · 前日 [`2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md`](2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md)。

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

### 4.2 建议探针形态（镜像 `fix-f203-alg19-kem-keygen-k4`）

| 项 | 建议 |
|----|------|
| **目录** | `ascendc-tests/fix-f203-alg20-kem-encaps-k4/` |
| **vendor 源** | [`fix-f203-alg14-pke-encrypt-correctness-k4`](../fix-f203-alg14-pke-encrypt-correctness-k4/) G5 全链（`vendor_sync.sh` 复制，禁止 `#include` 跨探针） |
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

Alg.21 为纯 **Alg.18 internal**（无新随机性）：`Decrypt` + `G` + 重加密比对 + implicit rejection `J(z‖c)`。可 vendor [`fix-f203-alg15`](../fix-f203-alg15-pke-decrypt-correctness-k4/) + 更厚的 KEM 尾段；**Encaps 探针应预留 `dk_kem` 3168B 布局解析**（`dk_pke‖ek‖H(ek)‖z`）供后续 Decaps 探针复用。

**TODO**：新开 **T7a**（或并入 T2）— Alg.20 探针 **规划**；写码前须用户确认目录 + customspec 不适用（`ascendc-tests` 探针）。

