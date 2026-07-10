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