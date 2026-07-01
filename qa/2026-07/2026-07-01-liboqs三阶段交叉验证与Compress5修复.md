# 2026-07-01 liboqs 三阶段交叉验证与 Compress_5 修复

关键词：**liboqs** · **PKE 三阶段交叉验证** · **`Compress_5` 舍入偏置** · **`SEED_D=20260619`** · **c₂ 分段定位**

> **TL;DR**：仓库根 [`scripts/liboqs_pke_vs_ascendc.sh`](../../scripts/liboqs_pke_vs_ascendc.sh) 下 KeyGen / Encrypt / Decrypt 分别与 liboqs **CPU+SIM max=0**。Encrypt 曾 FAIL 的根因是 **`Compress_5` 误用 `(1<<27)` 舍入偏置**（应为 `(1<<26)`），与 liboqs `mlk_scalar_compress_d5` / FIPS 203 一致；c₁（d=11）一直正确。定稿 note：[`docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](../../docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md)。

---

## 1. 背景与动机

| 层次 | 已有什么 | 缺什么 |
|------|----------|--------|
| 单探针 `run.sh` | device vs **host golden** max=0 | 不保证与 **liboqs** 一致 |
| [`roundtrip_pke_*`](../../scripts/roundtrip_pke_encrypt_decrypt.sh) | device c → device m 闭环 | 仍可能「自洽但偏离标准实现」 |
| **本任务** | liboqs 黑盒 fixture | KeyGen / Encrypt / Decrypt **分阶段字节对拍** |

固定种子 **`SEED_D=20260619`**；`m` / `coins` 与 Encrypt 探针 `gen_data` 同规则：`RNG(seed_d + 991)`。

---

## 2. 交付物（仓库根 `scripts/`）

| 文件 | 作用 |
|------|------|
| `liboqs_pke_ref.c` + `build_liboqs_pke_ref.sh` | liboqs PKE 黑盒（keygen / encrypt / decrypt） |
| `liboqs_pke_fixture.py` | 产出 `output/liboqs_pke_fixture/<SEED_D>/` |
| `liboqs_pke_vs_ascendc.sh` | 三阶段主编排 |
| `liboqs_pke_vs_ascendc_verify.py` | 字节对拍 |

**前置**：`thirdparty/liboqs` 已 build；`bash scripts/build_liboqs_pke_ref.sh`。

**验收**：

```bash
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash scripts/liboqs_pke_vs_ascendc.sh -r sim -v Ascend910B4
```

探针目录 [`INTEGRATION_PLAN.md`](../../ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/INTEGRATION_PLAN.md) §10 说明与探针边界（liboqs **不进**探针 `run.sh`）。

---

## 3. 排查过程（修前）

**首轮结果**：

| 阶段 | liboqs vs AscendC |
|------|-------------------|
| KeyGen ek/dk | **PASS** max=0 |
| Encrypt c | **FAIL** ~104 字节不同 |
| Decrypt | 未跑到 |

**关键观察**：

1. AscendC `c.bin` == host `golden_c.py`（max=0）→ 设备与 host golden **自洽**。
2. host golden `c.bin` != liboqs `c.bin` → 问题在 **golden / 设备 pack 语义**，非 SIM 同步。

**密文分段**（1568B = 1408B c₁ + 160B c₂）：

| 段 | 对拍 |
|----|------|
| c₁（u，`Compress_11` + ByteEncode₁₁） | **max=0** |
| c₂（v，`Compress_5` + ByteEncode₅） | **全部差异** |

pack 前 **v 系数**与 liboqs 一致 → 差异仅在 **`Compress_5` 编码**，不在 NTT / `Âᵀ·r̂` / `t̂·r̂` / CBD / `embed_message`。

**已排除**：CBD mod-Q vs signed（仅改 golden CBD 不能消除 c 差异）；KeyGen 仍与 liboqs 对齐说明 ek 链无阻塞。

---

## 4. 根因与修复

FIPS 203 `Compress_5` 定点实现（magic `1290176 = 2^5 · round(2^27/q)`）：

| 实现 | 舍入偏置 | 结果 |
|------|----------|------|
| **错误**（本仓 host + G4 pack） | `(d0 + (1<<27)) >> 27` | 约 116/256 系数与 liboqs 差 1 |
| **正确**（liboqs `mlk_scalar_compress_d5`） | `(d0 + (1<<26)) >> 27` | 与 FIPS 语义一致 |

`Compress_11` 使用 `(d0 + (1<<32)) >> 33`，故 c₁ 从未暴露此 bug。

**修改文件**：

| 路径 | 符号 |
|------|------|
| `fix-f203-alg14-.../scripts/host_golden/f203_ref_common.py` | `compress_d_scalar(d=5)` |
| `fix-f203-alg15-.../scripts/host_golden/f203_ref_common.py` | 同上（golden 共用） |
| `fix-f203-alg14-.../pack/f203_encrypt_pack_entry.cpp` | `compress_d5_u32` |
| `fix-f203-alg14-.../pack/compress_d_vec.hpp` | `scalar_compress_d5_u32` |

**运维注意**：改设备 pack 后须**重编** Encrypt 探针；`liboqs_pke_vs_ascendc.sh` 会复用已有 `ascendc_kernels_bbit`。

---

## 5. 验收结果（修后 · `SEED_D=20260619`）

| 模式 | KeyGen | Encrypt c | Decrypt m |
|------|--------|-----------|-----------|
| CPU | ek/dk max=0 | c max=0 | m max=0（vs liboqs m / m_rec / Encrypt input m） |
| SIM | 同左 | 同左 | 同左 |

日志：`[SUCCESS] liboqs PKE vs AscendC KeyGen+Encrypt+Decrypt (cpu|sim) SEED_D=20260619`。

Git：`4d549c3`（代码 + 脚本）；本纪要 + note + 探针 INTEGRATION_PLAN 为后续文档提交。

---

## 6. 决策要点

| 项 | 结论 |
|----|------|
| liboqs 与探针边界 | liboqs 仅 **仓库级** 交叉验证脚本；探针 `SELF_CONTAINED` 不变 |
| 三层验收关系 | ① 探针 vs host golden；② **liboqs 三阶段**；③ round-trip device 闭环 — 互补 |
| TODO | **T14b** 关闭（原「可选 liboqs KAT」已由三阶段脚本覆盖） |
