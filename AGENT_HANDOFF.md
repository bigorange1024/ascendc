# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-10（PKE 三段 stable + **统一整数 Compress/Decompress 验收** + **KEM KeyGen pass-fix**）

---

## ★ 强制写法（2026-07-08，新代码均须遵守）

| 类型 | 写法 | 禁止 |
|------|------|------|
| CPU vs SIM | `ASCENDC_BUILD_CPU` / `ASCENDC_BUILD_SIM` | per-probe 编译宏分叉 host |
| SIM host 拓扑 | `ASCENDC_SIM_HOST_MODE` + §3.3 登记表 | `F203_FEAS_*`、`KEM_DECAPS_SIM_2SESSION` 等 |
| 算法变体 | CMake 宏（CPU/SIM 同值） | env 切 launch |

详文：[`AscendC-CPU与SIM实现分叉开发指南.md`](docs/notes/AscendC-CPU与SIM实现分叉开发指南.md) §4.1 · [`library/shared/INDEX.md`](library/shared/INDEX.md)

**无 NPU 时验收权重**：全链 PKE — **SIM = 主参考**；**CPU = 辅助正确性**。

---

## ★ 当前真相（2026-07-10）

### PKE 三段 — **stable 交付齐备**

| 段 | stable | SIM tick（参考） |
|----|--------|------------------|
| KeyGen | [`stable-fips203-mlkem-pke-keygen-k4`](examples/stable/stable-fips203-mlkem-pke-keygen-k4/) | ~542k |
| Encrypt | [`stable-fips203-mlkem-pke-encrypt-k4`](examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) | ~627k |
| Decrypt | [`stable-fips203-mlkem-pke-decrypt-k4`](examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) | ~283k |

| 验收 | 结果 |
|------|------|
| PKE round-trip | `roundtrip_pke_batch.sh` **CPU×10 + SIM×1 PASS**（本地 golden） |
| 统一整数 Compress/Decompress | 已迁入 stable Encrypt tail + Decrypt unpack；exp 探针 + customspec |

纪要：[`qa/2026-07/2026-07-10-Decrypt交付stable.md`](qa/2026-07/2026-07-10-Decrypt交付stable.md)

### KEM Alg.19 KeyGen — **device PASS（pass-fix）**

| 路径 | 角色 |
|------|------|
| [`pass-fix-f203-alg19-kem-keygen-device-k4`](ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/) | **设备主线**（2 launch；stable PKE + 内嵌 Alg.16 尾） |
| [`fix-f203-alg19-kem-keygen-correctness-k4`](ascendc-tests/fix-f203-alg19-kem-keygen-correctness-k4/) | vendor oracle 对照（冻结） |

| 项 | 内容 |
|----|------|
| 验收 | CPU+SIM PASS；vs correctness 字节一致 |
| SIM | tick 均值 **~713k** |
| 脚本默认 | `KEYGEN_DIR` → **pass-fix**（`roundtrip_kem_*`、`liboqs_kem_vs_ascendc`、`kat_liboqs_kem_keygen`） |

### 统一整数 Compress/Decompress exp

| exp | 路径 |
|-----|------|
| Compress | [`exp-fips203-compress-unified-int-vec-k4`](examples/incubating/exp-fips203-compress-unified-int-vec-k4/) |
| Decompress | [`exp-fips203-decompress-unified-int-vec-k4`](examples/incubating/exp-fips203-decompress-unified-int-vec-k4/) |

---

## ★ 下一任务（P0）

**T19a — [`fix-f203-alg20-kem-encaps-device-k4`](ascendc-tests/fix-f203-alg20-kem-encaps-device-k4/)（KEM Encaps device）**

- 目标：改接 [`stable-fips203-mlkem-pke-encrypt-k4`](examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) 布局（或 pass-fix Alg.14 device）
- 输入：`ek_kem` ← **pass-fix** Alg.19 KeyGen
- 当前：`run.sh` exit 2；仍 vendor frozen G5

后继：**T19b/c** Alg.21 Decaps device → stable Decrypt fused。

**禁止**：从 frozen **抄码改写**；未改接线就把 vendor_sync SRC 指回 stable。

---

## 验收命令（smoke）

```bash
# PKE 全链（三段 stable）
bash scripts/roundtrip_pke_batch.sh

# KEM KeyGen device
cd ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# Decrypt stable 回归
cd examples/stable/stable-fips203-mlkem-pke-decrypt-k4
bash run.sh -r sim -v Ascend910B4
```

**WSL**：`CMAKE_BUILD_JOBS=2`；勿并行多 SIM。

---

## 附录：关键路径速查

| 主题 | 路径 |
|------|------|
| PKE 闭环 | [`scripts/roundtrip_pke_batch.sh`](scripts/roundtrip_pke_batch.sh) |
| KEM KeyGen device | [`pass-fix-f203-alg19-kem-keygen-device-k4/`](ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/) |
| KEM Encaps device（下一） | [`fix-f203-alg20-kem-encaps-device-k4/`](ascendc-tests/fix-f203-alg20-kem-encaps-device-k4/) |
| 统一整数总结 | [`docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md`](docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md) |
| 遗留总表 | [`qa/TODO.md`](qa/TODO.md) |
