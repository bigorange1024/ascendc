# 2026-07-10 — Decrypt 交付 stable

## 摘要

`exp-fips203-mlkem-pke-decrypt-k4` **`#交付#`** 复制晋级 → [`stable-fips203-mlkem-pke-decrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/)（T15a 关闭）。

## 前置验收（家里，2026-07-09 晚）

| 测试 | 结果 |
|------|------|
| KAT `CPU×10 + SIM×1` | PASS（`liboqs_pke_decrypt_fixture.py`：liboqs keygen + host golden_c，规避 `liboqs_pke_ref` encrypt/decrypt 链接） |
| roundtrip `CPU×10 + SIM×1` | PASS（`DECRYPT_DIR`→exp） |

## 晋级操作

- `rsync` 复制 exp → stable（排除 build/output/sim_log）
- customspec 重命名并编译 PDF：`stable-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.{tex,pdf}`
- 刷新 `examples/stable/INDEX.md`、`examples/incubating/INDEX.md`、`examples/INDEX.md`
- `scripts/roundtrip_pke_encrypt_decrypt.sh`、`liboqs_pke_vs_ascendc.sh` 默认 `DECRYPT_DIR` → stable
- `scripts/liboqs_pke_decrypt_fixture.py` host_golden 路径 → stable

## PKE 闭环默认（三段均 stable）

| 段 | 路径 |
|----|------|
| KeyGen | `stable-fips203-mlkem-pke-keygen-k4` |
| Encrypt | `stable-fips203-mlkem-pke-encrypt-k4` |
| Decrypt | `stable-fips203-mlkem-pke-decrypt-k4` |

## ascendc-tests 与 GitHub 对齐

删除仅本地存在、`origin/main` 无的残留：

- `fix-f203-alg14-encrypt-2launch-k4`（远端已在 `frozen/`）
- `fix-f203-{alg6-bytedecode,byteencode,compress,decompress}-d-vec-k4`（已有同名 `pass-*`）
- 幽灵壳 `examples/incubating/exp-mlkem-f203-pke-keygen-k4/`（改名残留）
- 若干空壳 / 未跟踪产物目录

对齐后活跃目录数与 `origin/main` 均为 **31**。

## TODO 刷新

**PKE 三段主要算子均已 stable 交付**，`qa/TODO.md` 收口：

| 段 | stable | 关闭项 |
|----|--------|--------|
| Alg.13 KeyGen | `stable-fips203-mlkem-pke-keygen-k4` | T13h |
| Alg.14 Encrypt | `stable-fips203-mlkem-pke-encrypt-k4` | T14a |
| Alg.15 Decrypt | `stable-fips203-mlkem-pke-decrypt-k4` | **T15a** |

- 打开项主线切 **KEM Alg.19–21**（T6 / T7a / T7c）；T7b 降为 P1 工程债
- 新增 **T18**（Encrypt `liboqs_pke_ref` 链接债，非阻塞）

## 索引死链清理（同日推送前）

- `frozen-pass-fix-…-halfbatch` → 实际目录 `frozen-fix-…-halfbatch`
- 缺失 `qa/2026-06-19-ByteEncode12-only…` → 补桩文件指向定稿 notes
- `docs-archiving.mdc` / `offline-web` / `compare_stage2_logs.py` 等 INDEX 死链改指向现存路径或删除行

---

## 冻结 alg14/15 correctness 探针（同日续）

| 原活跃目录 | 冻结路径 | 继任 |
|------------|----------|------|
| `fix-f203-alg14-pke-encrypt-correctness-k4` | `frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/` | `stable-fips203-mlkem-pke-encrypt-k4` |
| `fix-f203-alg15-pke-decrypt-correctness-k4` | `frozen/frozen-fix-f203-alg15-pke-decrypt-correctness-k4/` | `stable-fips203-mlkem-pke-decrypt-k4` |

原因：**正确性验证任务已完成**。全仓调用/`ENCRYPT_DIR`/`DECRYPT_DIR`/文档链接改指向 stable；历史深链（如 `G3_SIM_AUDIT.md`）改指向 frozen 归档。

### 更正：KEM alg20/21 的 vendor_sync **不能**改指向 stable

冻结时曾把 `fix-f203-alg20/21` 的 `vendor_sync` SRC 改成 stable，**布局不兼容**：

| 探针需要 | stable | frozen correctness |
|----------|--------|--------------------|
| Encrypt `pack/` + G5 host | **无** | **有** |
| Decrypt **2-launch G4** | 1-kernel fused | **有** |

**已改回**：alg20/21 `vendor_sync` → `frozen-fix-f203-alg14/15-*-correctness-k4`（仅拼装快照；`ENCRYPT_DIR`/`DECRYPT_DIR` 仍用 stable）。`FROZEN.md` / `frozen/INDEX.md` 记明该例外。

### 待办：KEM 算子重构对齐 stable（**T19**）

后续须重构 KEM 探针接线，使 vendor **吃 stable PKE**，不再依赖 frozen G5/G4：

| ID | 内容 |
|----|------|
| **T19a** | Alg.20 Encaps ← stable Encrypt 布局 |
| **T19b** | Alg.21 Phase-E ← 同上 |
| **T19c** | Alg.21 Phase-D ← stable Decrypt fused（或改 host） |
| **T19d** | Alg.19 复核（已 vendor stable KeyGen） |
| **T19e** | 去掉 frozen 作 KEM sync 源；刷新 FROZEN/notes |

总表：[`qa/TODO.md`](../TODO.md) **T19**。在此之前 **禁止** 再把 `vendor_sync` SRC 静默改回 stable。

### 活跃用例中文注释补课（**T20**，同日）

| 决策 | 内容 |
|------|------|
| **1A** | 跳过 `vendor/`（sync 会覆盖；注释改在 sync 源 / T19 后 stable） |
| **2①** | Wave1：`examples/stable` 三段 PKE + `fix-f203-alg19/20/21` 自有代码 |
| 排除 | `frozen/`、`add_custom/`、`thirdparty/`、构建产物 |
| 标准 | 文件头 + 函数头（I/O/形状）+ 函数体分段中文；只加注释不改逻辑 |

后续波次：W2 `pass-fix` PKE device → W3 NTT/积木 → W4 小探针 → W5 incubating。总表 **T20**。

#### Wave1 完成（同日）

| 目录 | 说明 |
|------|------|
| stable keygen / encrypt / decrypt | 自研源文件补文件头+函数头+分段中文；LUT 巨表仅文件头/用途 |
| fix-alg19/20/21 | **仅自有** `kem/`、main、scripts；**未改** `vendor/` |
| 验收 | 密度抽查 thin≈0；`git diff` 无 `vendor/`；仅注释不改逻辑 |

下一会话开 **T20-W2**。

#### Wave2–5 完成（同日续）

| 波次 | 范围 | 规模 |
|------|------|------|
| **W2** | `pass-fix` alg13/14/15 device、encrypt prep/compute/pack 分段 | ~470 文件 |
| **W3** | vec-k4-v2、stage123、innerproduct、multiplyntts、byteencode12 | ~130 文件 |
| **W4** | compress/decompress/byteencode/bytedecode、alg7/8、shake/toy | ~86 文件 |
| **W5** | `examples/incubating/exp-*` | ~275 文件 |

验收：`git diff` **无** `vendor/`、`thirdparty/`、`frozen/`；仅注释不改逻辑；commit `docs(comments): T20-W2–W5 …`。

#### T20-W2 片段（同日，pass-fix 9 文件）

仅加中文注释、不改逻辑；对照 `examples/stable` 同名风格：

| 路径 |
|------|
| `pass-fix-f203-alg14-pke-encrypt-device-k4/prep/alg7/f203_alg7_compact_lut.h` |
| `pass-fix-f203-alg14-pke-encrypt-device-k4/prep/ahat/data_utils.h` |
| `pass-fix-f203-alg15-pke-decrypt-device-k4/data_utils.h` |
| `pass-fix-f203-alg15-pke-decrypt-device-k4/compute/intt_w/f203_decrypt_intt_w_entry.cpp` |
| `pass-fix-f203-alg15-pke-decrypt-device-k4/compute/su_dot/f203_decrypt_su_dot_kernel.cpp` |
| `pass-fix-f203-alg15-pke-decrypt-device-k4/compute/ntt_u/aiv_func.hpp` |
| `pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/prep/alg7/f203_alg7_compact_lut.h` |
| `pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/prep/ahat/data_utils.h` |
| `pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/main.cpp` |

验收：`intt_w` / `su_dot` 去注释后与 stable 同构；LUT 仅文件头+表用途。

---

## §11 KEM 探针拆 correctness / device-k4（2026-07-10 晚）

**决策**：现有三 KEM 探针**只改名、逻辑不动** → `*-correctness-k4`；新建 `*-device-k4` 供 **T19** 去 vendor 重构。

| 原目录 | correctness（可跑） | device（占位） |
|--------|---------------------|----------------|
| alg19 KeyGen | [`fix-f203-alg19-kem-keygen-correctness-k4`](../../ascendc-tests/fix-f203-alg19-kem-keygen-correctness-k4/) | [`…-device-k4`](../../ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/) |
| alg20 Encaps | [`fix-f203-alg20-kem-encaps-correctness-k4`](../../ascendc-tests/fix-f203-alg20-kem-encaps-correctness-k4/) | [`pass-fix-…-device-k4`](../../ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/) |
| alg21 Decaps | [`fix-f203-alg21-kem-decaps-correctness-k4`](../../ascendc-tests/fix-f203-alg21-kem-decaps-correctness-k4/) | [`…-device-k4`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/) |

- 仓库 `scripts/roundtrip_kem_*` / `liboqs_kem_vs_ascendc.sh`：当时默认仍指向 correctness-k4；**2026-07-15**：Encaps 默认已改指 [`pass-fix-…-encaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/)（KeyGen 早已 pass-fix；Decaps 仍 correctness 直至 T19b/c）。
- frozen `FROZEN.md` vendor 例外路径已改为 correctness-k4。
- device-k4 当前 `run.sh` exit **2**（未实现）。*（注：Alg.20 Encaps device 其后已实现并更名 pass-fix；本段保留 07-10 当日快照。）*

---

## §12 Alg.19 device-k4 实现方案（2 launch · 2026-07-10）

**决策**：[`pass-fix-f203-alg19-kem-keygen-device-k4/INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/INTEGRATION_PLAN.md) — **Alg.19→Alg.16→Alg.13 串链，Launch 预算 = stable PKE（2 次）**。

| 要点 | 内容 |
|------|------|
| L1 | stable `f203_keygen_prep`（Alg.13 行 3–15，`d` UB） |
| L2 | stable `mmad_custom` + **内嵌** `KemKgTailFused`（`H(ek)`+`z`+拼 `dk_kem`） |
| 零拷贝 | `ek_kem_gm` 别名 `ek_pke_gm`；仅新增 `dk_kem_gm[3168]` |
| PKE 源 | 编译期引用 stable，无 `vendor/` |
| 对照 | correctness 3 launch 作密码学 oracle |

写码 checklist 见 INTEGRATION_PLAN §11；**T19d CPU+SIM PASS**（P1 后 tick 均值 **713227**，与 correctness 字节一致）。

## §13 Alg.19 device-k4 P1 定案保留（2026-07-10）

**决策**：保留 **stable `F203_KEM_KEYGEN_TAIL` 宏 + 本地 ROM**，废弃 `mmad_custom_kem.cpp` fork。

| 项 | 结论 |
|----|------|
| tick | fork 首期 700718 → P1 后 3 次均值 **713227**（+~1.8%），极差 24 |
| I/O | 与 correctness **cmp 一致** |
| 保留理由 | 无 mmad merge 债、ROM 不污染 stable、PKE 宏默认 0 零回归 |
| SHA3 | 尾段两处 SHA3-256 在 `kem/`；日后第三方 AscendC 可替换，见 INTEGRATION_PLAN §4.5 |

文档：[`STATUS.md`](../../ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/STATUS.md) · [`INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/INTEGRATION_PLAN.md) §4.5

## §14 Compress/Decompress 统一整数舍入定稿（2026-07-10）

**要点**：`round(u·2^d/q)` → 乘 `y=2^k` 消分母；选 **`2^37/3329≈41285357`** 为统一 `C`；每档 **`y=2^(37-d)`**。全程乘加移位，**无除法/浮点** → CT 友好 + AscendC 全 d 可向量化（可替代 d=10/11 cast_div）。

定稿：[`docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md`](../../docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md)

## §15 统一整数 Compress/Decompress 探针落地（2026-07-10）

| 探针 | 路径 | 设备路径 | 验收 |
|------|------|----------|------|
| Compress | [`exp-fips203-compress-unified-int-vec-k4`](../../examples/incubating/exp-fips203-compress-unified-int-vec-k4/)（自 pass 迁入） | **int32 向量 limb** | 全 d CPU+SIM；customspec PDF |
| Decompress | [`exp-fips203-decompress-unified-int-vec-k4`](../../examples/incubating/exp-fips203-decompress-unified-int-vec-k4/) | int32 全向量 | 全 d CPU；SIM d=4/11 |

共享：`library/shared/f203_unified_round/`。

## §16 统一整数向量 Compress/Decompress 生产验收（2026-07-10）

| 项 | 结果 |
|----|------|
| stable Encrypt tail | Barrett/float → **统一 limb**（d=5/11） |
| stable Decrypt unpack | 统一整数 Decompress 命名对齐 |
| PKE round-trip | `roundtrip_pke_batch.sh` **CPU×10 + SIM×1 PASS**（本地 golden，无 liboqs） |
| 向量 Compress/Decompress 验收 | **通过**（round-trip 覆盖生产路径） |
| exp 迁入 | `pass-f203-compress/decompress-unified-int-vec-k4` → `examples/incubating/exp-fips203-*` + customspec |

## §17 KEM KeyGen device 更名 pass-fix（2026-07-10）

| 项 | 内容 |
|----|------|
| 更名 | `fix-f203-alg19-kem-keygen-device-k4` → **`pass-fix-f203-alg19-kem-keygen-device-k4`** |
| T19e | `scripts/` KeyGen 默认改 **pass-fix**（`roundtrip_kem_*`、`liboqs_kem_vs_ascendc`、`kat_liboqs_kem_keygen`） |
| 下一 P0 | **T19a** [`pass-fix-f203-alg20-kem-encaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/)（KEM Encaps） |

## §18 统一整数 Compress §8 业界对比定稿（2026-07-10 续）

**背景**：统一整数舍入（§14–§16）定稿后，补充与 mlkem-native / OpenSSL Barrett / Botan / US 11,632,242（T=35）等路线的**优缺点与选型**说明，便于对外表述与评审。

| 项 | 结论 |
|----|------|
| 位置 | [`docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md`](../../docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md) **§8** |
| 定位 | 属「2^T/q 乘加移位」成熟技术族；**Decompress 与业界标准式相同**；差异化在 **Compress 全 d 同构（T=37, C=41285357）** |
| 相对分档 magic | 换 per-d 最优移位/窄中间量 → **单一 C + 派生 k/bias**、AscendC 不分 float/int 叉、CT 叙述统一 |
| 相对 US 11,632,242 | 同族「统一乘数 + 按 d 变移位」；本仓 **T=37**、显式 `bias=2^(36-d)`，非「首个统一 Compress」 |
| 本仓优先场景 | AscendC 向量、stable/tail 共用 `library/shared/f203_unified_round/`、交付 CT 门禁 |
| 仍可保留分档/float | 极致 CPU 单档、16 位 MCU、与 liboqs 字节级同源审计 |

详表与外部链接：定稿 **§8.1–§8.7**；向量指南已链到 §8。