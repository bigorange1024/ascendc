# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-06-29（晚 · Encrypt G5）

---

## 0. 家里 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull`（**主代码源**是 GitHub，不是 `backup/`） |
| 2 | 读本文件 §1–§3，**尤其 §Encrypt** |
| 3 | [`README.md`](README.md) → [`qa/TODO.md`](qa/TODO.md) → [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 活跃探针以 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) 为准；**禁止**从 `frozen/` 抄码 |

**勿再读** `HOME-KEYGEN-DEBUG.md`（已删；内容已收敛到定稿 note + 当日 qa）。

---

## 1. 当前真相（2026-06-29 晚）

### Encrypt Alg.14（**明日汇报优先**）

| 路径 | [`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) |
|------|------|
| **默认** | `ENCRYPT_GATE=5`（设备 decode，无 `input/t_hat.bin`） |
| **CPU 全链** | ✅ `ENCRYPT_VERIFY=1` → c.bin max=0（**可 demo**） |
| **SIM G1–G3** | ✅ `verify_gate` max=0（含 G5 device decode + 双 session tr̂） |
| **SIM 全链** | ❌ `ENCRYPT_VERIFY=1` c.bin FAIL @382（G4/G5 共用 INTT/noise/pack tail） |

**明日保底 demo**：

```bash
cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
ENCRYPT_VERIFY=1 ENCRYPT_GATE=5 bash run.sh -r cpu -v Ascend910B4
```

**今晚/家里必做**：修 SIM `c.bin`（见 [`STATUS.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) · [`G3_SIM_AUDIT.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/G3_SIM_AUDIT.md) §9.9–§10）。**禁止**回退 Host `t_hat.bin` / fake-Â。

**纪要**：[`qa/2026-06/2026-06-29-KeyGen双AIV并行fork探针.md`](qa/2026-06/2026-06-29-KeyGen双AIV并行fork探针.md) §G5

### KeyGen（k=4，生产路径）

| 角色 | 路径 | 状态 |
|------|------|------|
| **stable 交付** | [`examples/stable/stable-mlkem-f203-pke-keygen-k4/`](examples/stable/stable-mlkem-f203-pke-keygen-k4/) | CPU/SIM/KAT ✅；SIM **542393** tick |
| **探针** | [`ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/`](ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) | 同上（prep **双 AIV 并行 Â**） |
| **incubating 副本** | [`examples/incubating/exp-mlkem-f203-pke-keygen-k4/`](examples/incubating/exp-mlkem-f203-pke-keygen-k4/) | 保留；验收以 **stable** 为准 |
| **旧 pass（串行 Â）** | [`ascendc-tests/frozen/frozen-fix-f203-alg13-device-keygen-k4/`](ascendc-tests/frozen/frozen-fix-f203-alg13-device-keygen-k4/) | **已关闭**；只读 `FROZEN.md` |

**T13h（双 AIV 并行 Â）**：✅ 完成。

**定稿**：[`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)

### ML-KEM NTT / 2s1e 向量基线

| 路径 | 说明 |
|------|------|
| [`ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/`](ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) | Alg.13 行 16–20 全链；SIM **77958** tick |
| [`ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/`](ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) | 8-poly NTT/INTT；NTT **30347** / INTT **30340** |

### 运行约定（默认，勿手动 export）

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4          # run.sh 内自动 SIM_DIRECT=1
bash kat_liboqs_vs_ascendc.sh             # KeyGen：CPU×10 + SIM×1
```

CPU `[SUCCESS][AIC_*]` 为 tikicpu 伪影；**以 SIM `profile_subtask_log*.toml` 为准**。

---

## 2. GitHub 同步（家里拉代码）

**推荐**：`git pull origin main` — 应包含 **Encrypt 探针整树** + G5 代码。

### 应上传（track）

| 类别 | 路径 |
|------|------|
| 探针 / 算子 | `ascendc-tests/**`（含 `fix-f203-alg14-pke-encrypt-correctness-k4/`、`frozen/`） |
| vendored LUT | `**/thirdparty/ntt_study/**` |
| 共享库 | `library/shared/**` |
| 脚本 | `scripts/**` |
| 文档 | `docs/**`、`qa/**` |
| Agent | `AGENT_HANDOFF.md`、`README.md`、`.cursor/rules/`、`.cursor/skills/` |

### 勿上传（`.gitignore`）

`backup/`、`/thirdparty/liboqs`、`**/build/`、`out/`、`input/`、`output/`、`*.bin` 等。

---

## 3. 本地备份（辅助，非主源）

```bash
bash backup-project.sh   # → backup/v0.1_YYYYMMDDHHMMSS/
```

---

## 4. 建议下一步（见 [`qa/TODO.md`](qa/TODO.md)）

| 优先级 | ID | 事项 |
|--------|-----|------|
| **P0** | **T14** | **Encrypt G5 SIM 全链** — 修 G4 tail c.bin；CPU 已 PASS |
| 1 | T13b | vec-k4-v3（设备 `a_hat` + V3 预采样） |
| 2 | T11 | 2s1e exp → stable 晋级 |

---

## 5. 家里 smoke（拉代码后）

```bash
# P0：Encrypt CPU 全链（应 PASS）
cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
ENCRYPT_VERIFY=1 ENCRYPT_GATE=5 bash run.sh -r cpu -v Ascend910B4

# KeyGen
cd ../../examples/stable/stable-mlkem-f203-pke-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh
```

---

## 6. 索引

| 主题 | 链接 |
|------|------|
| Encrypt STATUS | [`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) |
| G3/G5 审计 | [`G3_SIM_AUDIT.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/G3_SIM_AUDIT.md) |
| 活跃探针 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| KeyGen 原理 | [`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md) |
