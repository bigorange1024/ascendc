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
