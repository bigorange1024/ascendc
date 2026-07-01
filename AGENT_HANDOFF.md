# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-06-30（夜 · **liboqs PKE 三阶段 CPU+SIM max=0** + `Compress_5` 修复）

---

## ★ 验收声明（2026-06-30）

### liboqs PKE 三阶段交叉验证（**新增 · 权威外部 oracle**）

**结论**：KeyGen / Encrypt / Decrypt 分别与 liboqs **字节对拍 max=0**（`SEED_D=20260619`）。

| 项 | 内容 |
|----|------|
| 脚本 | [`scripts/liboqs_pke_vs_ascendc.sh`](scripts/liboqs_pke_vs_ascendc.sh) |
| 依赖 | `thirdparty/liboqs` 已 build；`bash scripts/build_liboqs_pke_ref.sh` |
| CPU | 三阶段 **PASS**（ek/dk 1568+1536B · c 1568B · m 32B） |
| SIM | 三阶段 **PASS**（`SIM_DIRECT=1`） |

```bash
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash scripts/liboqs_pke_vs_ascendc.sh -r sim -v Ascend910B4
```

**根因（Encrypt c 曾 FAIL）**：`Compress_5` 定点舍入误用 `(1<<27)`，应为 liboqs/FIPS 203 的 `(1<<26)`；c₁（d=11）一直正确。详 [`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](qa/2026-06/2026-06-30-funckey-507000本地独立验证.md) §17。

### Encrypt

**结论**：`fix-f203-alg14-pke-encrypt-correctness-k4` CPU+SIM **c.bin max=0**（host golden + liboqs 均已对齐）。

| 项 | 内容 |
|----|------|
| 命令 | `bash run.sh -r sim -v Ascend910B4` |
| 关键日志 | `[verify] PASS max=0 (1568 bytes)` |
| SIM `Total tick` | **922441**（507000 病根已治愈） |

### Decrypt

**结论**：`fix-f203-alg15-pke-decrypt-correctness-k4` CPU+SIM G1–G4 **m.bin max=0**。

| 项 | 内容 |
|----|------|
| SIM `Total tick` | **~427k** |
| 定稿 note | [`docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md`](docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md) |

### PKE round-trip（device 闭环）

**结论**：KeyGen 密钥 → device Encrypt `c.bin` → device Decrypt `m.bin`；**CPU+SIM max=0**。

| 脚本 | [`scripts/roundtrip_pke_encrypt_decrypt.sh`](scripts/roundtrip_pke_encrypt_decrypt.sh) |

> **两层验收**：① 各探针 `run.sh` vs host golden；② **`liboqs_pke_vs_ascendc`** vs liboqs；③ **`roundtrip_pke_*`** 跨算子 device I/O。三者互补。

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull`（**主代码源**是 GitHub，不是 `backup/`） |
| 2 | 读本文件 §1 |
| 3 | [`README.md`](README.md) → [`qa/TODO.md`](qa/TODO.md) → [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 活跃探针以 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) 为准；**禁止**从 `frozen/` 抄码 |

---

## 1. 当前真相（2026-06-30 夜）

### Encrypt Alg.14（G5 · liboqs 对齐）

| 路径 | [`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) |
|------|------|
| **验收** | `run.sh` CPU+SIM **c.bin max=0**；**liboqs c max=0** |
| **G3** | **`at_r5` 合并核**（kP=5） |
| **pack 修复** | `compress_d5` 舍入偏置 `(1<<26)`（`f203_encrypt_pack_entry.cpp` / `compress_d_vec.hpp` / host golden） |

### Decrypt Alg.15（G4 2-launch PASS）

| 路径 | [`ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/`](ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/) |
|------|------|
| **验收** | CPU+SIM **m.bin max=0**；**liboqs m max=0** |
| **Launch** | **2×** `aclrtLaunchKernel`：prep → ntt+intt |

### KeyGen（k=4）

| 角色 | 路径 | liboqs |
|------|------|--------|
| **stable** | [`examples/stable/stable-mlkem-f203-pke-keygen-k4/`](examples/stable/stable-mlkem-f203-pke-keygen-k4/) | KAT ✅ |
| **探针** | [`ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/`](ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) | **ek/dk max=0** |

### 运行约定（默认，勿手动 export）

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

---

## 2. GitHub 同步

**推荐**：`git pull origin main`

### `thirdparty/` — 本地依赖（**不进 GitHub**）

| 目录 | 用途 |
|------|------|
| `liboqs/` | **`liboqs_pke_vs_ascendc`** / KAT |
| `ntt_study/` | golden 对照 |
| `tiny_sha3/` | Host SHA3/SHAKE |

**liboqs 交叉验证前**：`bash scripts/build_liboqs_pke_ref.sh`

---

## 3. 建议下一步（见 [`qa/TODO.md`](qa/TODO.md)）

| 优先级 | ID | 事项 |
|--------|-----|------|
| P1 | **T13b** | vec-k4-v3（V3 预采样 + 设备 `a_hat`） |
| P2 | **T11** | 2s1e exp → stable 晋级 |
| P3 | **T14a** | Encrypt G5 → stable 晋级 |
| P4 | **T15a** | Decrypt G4 → stable 晋级 |

---

## 4. smoke（拉代码后）

```bash
# liboqs 三阶段（需 thirdparty/liboqs + build_liboqs_pke_ref.sh）
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4

cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
bash run.sh -r cpu -v Ascend910B4

cd ../fix-f203-alg15-pke-decrypt-correctness-k4
bash run.sh -r cpu -v Ascend910B4

bash ../../scripts/roundtrip_pke_encrypt_decrypt.sh -r cpu -v Ascend910B4

cd ../../examples/stable/stable-mlkem-f203-pke-keygen-k4
bash kat_liboqs_vs_ascendc.sh
```

---

## 5. 索引

| 主题 | 链接 |
|------|------|
| liboqs 交叉验证纪要 | [`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](qa/2026-06/2026-06-30-funckey-507000本地独立验证.md) §17 |
| Encrypt STATUS | [`fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) |
| Decrypt STATUS | [`fix-f203-alg15-pke-decrypt-correctness-k4/STATUS.md`](ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/STATUS.md) |
| 活跃探针 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
