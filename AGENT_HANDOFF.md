# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-07-02（**KEM Alg.19 KeyGen T6 PASS** · 详 [`qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md`](qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md)）

---

## ★ 下一任务（家里 Agent · P0）

### ML-KEM Alg.17/18 Encaps/Decaps（**未建探针**）

| 项 | 内容 |
|----|------|
| **前置** | PKE Encrypt/Decrypt ✅ · **KEM Alg.19 KeyGen** ✅（[`fix-f203-alg19-kem-keygen-k4`](ascendc-tests/fix-f203-alg19-kem-keygen-k4/)） |
| **TODO** | 独立探针规划（非本目录） |

---

## ★ 验收声明（KEM Alg.19 · 已完成）

### ML-KEM Alg.19 KeyGen 正确性探针

**结论**：`ek_kem`/`dk_kem` 与 liboqs **字节对拍 max=0**（`SEED_D=20260619`）；`d`/`z` device UB 生成、不导出。

| 项 | 内容 |
|----|------|
| **目录** | [`ascendc-tests/fix-f203-alg19-kem-keygen-k4/`](ascendc-tests/fix-f203-alg19-kem-keygen-k4/) |
| **方案** | [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg19-kem-keygen-k4/INTEGRATION_PLAN.md) · [`STATUS.md`](ascendc-tests/fix-f203-alg19-kem-keygen-k4/STATUS.md) |
| **原理** | [`docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md`](docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md) |
| **SIM tick** | **742558** |

```bash
cd ascendc-tests/fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
bash ../../scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash ../../scripts/liboqs_kem_vs_ascendc.sh -r sim -v Ascend910B4
```

---

### liboqs PKE 三阶段交叉验证

**结论**：KeyGen / Encrypt / Decrypt 分别与 liboqs **字节对拍 max=0**（`SEED_D=20260619`）。

```bash
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash scripts/liboqs_pke_vs_ascendc.sh -r sim -v Ascend910B4
```

详 [`docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md) · **`Compress_5` `(1<<26)`** 修复。

### PKE 探针 + round-trip

| 算子 | 路径 | 验收 |
|------|------|------|
| Encrypt | [`fix-f203-alg14-pke-encrypt-correctness-k4`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) | CPU+SIM c max=0 · SIM **922441** tick |
| Decrypt | [`fix-f203-alg15-pke-decrypt-correctness-k4`](ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/) | CPU+SIM m max=0 · **2 launch** |
| KeyGen stable | [`stable-mlkem-f203-pke-keygen-k4`](examples/stable/stable-mlkem-f203-pke-keygen-k4/) | liboqs ek/dk max=0 |
| Round-trip | [`scripts/roundtrip_pke_encrypt_decrypt.sh`](scripts/roundtrip_pke_encrypt_decrypt.sh) | device 闭环 CPU+SIM |

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull`（**主代码源**是 GitHub，不是 `backup/`） |
| 2 | 读本文件 **§下一任务** |
| 3 | [`README.md`](README.md) → [`qa/TODO.md`](qa/TODO.md) → [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 读探针 [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg19-kem-keygen-k4/INTEGRATION_PLAN.md) 再写码 |
| 5 | 活跃探针以 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) 为准；**禁止**从 `frozen/` 抄码 |

---

## 1. 运行约定（默认，勿手动 export）

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

kernel 计算默认 **≤15s**（`KERNEL_COMPUTE_BUDGET_SEC`）。

---

## 2. GitHub 同步

**推荐**：`git pull origin main`

### `thirdparty/` — 本地依赖（**不进 GitHub**）

| 目录 | 用途 |
|------|------|
| `liboqs/` | PKE/KEM liboqs 交叉验证 |
| `tiny_sha3/` | Host golden **仅** VERIFY 路径 |

**liboqs 前**：`bash scripts/build_liboqs_pke_ref.sh`

---

## 3. smoke（拉代码后 · PKE 回归）

```bash
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4

cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4 && bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg15-pke-decrypt-correctness-k4 && bash run.sh -r cpu -v Ascend910B4
bash ../../scripts/roundtrip_pke_encrypt_decrypt.sh -r cpu -v Ascend910B4
```

KEM 探针 smoke：

```bash
cd ascendc-tests/fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4
bash ../../scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4
```

---

## 4. 索引

| 主题 | 链接 |
|------|------|
| 当日纪要 | [`qa/2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md`](qa/2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md) |
| KEM Alg.19 note | [`docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md`](docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md) |
| KEM 探针 STATUS | [`fix-f203-alg19-kem-keygen-k4/STATUS.md`](ascendc-tests/fix-f203-alg19-kem-keygen-k4/STATUS.md) |
| 活跃探针 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| 遗留总表 | [`qa/TODO.md`](qa/TODO.md) |
