# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-06-29

---

## 0. 家里 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull`（**主代码源**是 GitHub，不是 `backup/`） |
| 2 | 读本文件 §1–§3 |
| 3 | [`README.md`](README.md) → [`qa/TODO.md`](qa/TODO.md) → [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 活跃探针以 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) 为准；**禁止**从 `frozen/` 抄码 |

**勿再读** `HOME-KEYGEN-DEBUG.md`（已删；内容已收敛到定稿 note + 当日 qa）。

---

## 1. 当前真相（2026-06-29）

### KeyGen（k=4，生产路径）

| 角色 | 路径 | 状态 |
|------|------|------|
| **stable 交付** | [`examples/stable/stable-mlkem-f203-pke-keygen-k4/`](examples/stable/stable-mlkem-f203-pke-keygen-k4/) | CPU/SIM/KAT ✅；SIM **542393** tick |
| **探针** | [`ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/`](ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) | 同上（prep **双 AIV 并行 Â**，非 block0 串行） |
| **incubating 副本** | [`examples/incubating/exp-mlkem-f203-pke-keygen-k4/`](examples/incubating/exp-mlkem-f203-pke-keygen-k4/) | 保留；验收以 **stable** 为准 |
| **旧 pass（串行 Â）** | [`ascendc-tests/frozen/frozen-fix-f203-alg13-device-keygen-k4/`](ascendc-tests/frozen/frozen-fix-f203-alg13-device-keygen-k4/) | **已关闭**；只读 `FROZEN.md` |

**T13h（双 AIV 并行 Â）**：✅ 完成；旧串行 pass 已 frozen。

**定稿**：[`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md) · 讨论 [`qa/2026-06/2026-06-29-KeyGen双AIV并行fork探针.md`](qa/2026-06/2026-06-29-KeyGen双AIV并行fork探针.md)

### ML-KEM NTT / 2s1e 向量基线

| 路径 | 说明 |
|------|------|
| [`ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/`](ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) | Alg.13 行 16–20 全链；SIM **77958** tick |
| [`ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/`](ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) | 8-poly NTT/INTT；NTT **30347** / INTT **30340** |

**陷阱（已清理）**：仓库**根目录**下**不存在** `pass-fix-f203-*` 探针；若在 `~/ascendc/pass-fix-...` 看到空壳，是误建目录，已删。

### 运行约定（默认，勿手动 export）

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4          # run.sh 内自动 SIM_DIRECT=1
bash kat_liboqs_vs_ascendc.sh             # KeyGen：CPU×10 + SIM×1
```

CPU `[SUCCESS][AIC_*]` 为 tikicpu 伪影；**以 SIM `profile_subtask_log*.toml` 为准**。

---

## 2. GitHub 同步（家里拉代码）

**推荐**：`git pull origin main` — 应包含完整活跃源码树。

### 应上传（track）

| 类别 | 路径 |
|------|------|
| 探针 / 算子 | `ascendc-tests/**`（含 `frozen/` 判决书）、`examples/**` |
| vendored LUT | `**/thirdparty/ntt_study/**`（在探针/example **内部**，非仓库根） |
| 共享库 | `library/shared/**` |
| 脚本 | `scripts/**` |
| 文档 | `docs/**`、`qa/**` |
| Agent | `AGENT_HANDOFF.md`、`README.md`、`.cursor/rules/`、`.cursor/skills/` |
| C 演示 | `src/`、`include/`、`Makefile` |

### 勿上传（`.gitignore`）

| 路径 | 原因 |
|------|------|
| `backup/` | 本地快照；易过时；非主源 |
| `/thirdparty/`（**仅仓库根**） | `liboqs` 等大体积；KAT 用 `scripts/build_liboqs_pke_ref.sh` 本机构建 |
| `packages/`、`samples/` | 外部 CANN 样例 |
| `**/build/`、`out/`、`input/`、`output/`、`sim_log/`、`OPPROF_*`、`*.bin` | 产物 |

**2026-06-29 修复**：`.gitignore` 原为 `thirdparty/` 会误伤探针内 vendored LUT → 已改为 `/thirdparty/`（仅根目录）。**推送前请 `git add` 各探针 `thirdparty/ntt_study/`**。

### liboqs KAT（家里首次）

```bash
# 仓库根 thirdparty/liboqs 不在 Git 内
git clone --branch 0.15.0 --depth 1 https://github.com/open-quantum-safe/liboqs.git thirdparty/liboqs
cd examples/stable/stable-mlkem-f203-pke-keygen-k4 && bash scripts/build_liboqs_pke_ref.sh
```

---

## 3. 本地备份（辅助，非主源）

```bash
bash backup-project.sh   # → backup/v0.1_YYYYMMDDHHMMSS/
```

含 `ascendc-tests/`、`examples/` 全 vendored 树、`scripts/`、`.cursor/`、`AGENT_HANDOFF.md`。  
**不含** `backup/` 自身、`*.bin`、build 产物、根 `thirdparty/liboqs`。

若家里只有旧备份（如 `v0.1_20260626191947`）：**缺** `scripts/`、大量 vendored、`stable/` — **请改 pull GitHub**。

最新本地快照（办公室）：`backup/v0.1_20260629140010/`（1565 文件）。

---

## 4. 建议下一步

见 [`qa/TODO.md`](qa/TODO.md)。摘要：

1. **T13b** — vec-k4-v3（设备 `a_hat` + V3 预采样接入 2s1e 全链）
2. **T11** — 2s1e exp → stable 晋级（`exp-mlkem-f203-alg13-16171820-2s1e-k4`）
3. **Encrypt fix 探针** — compress/decompress/byteencode 推 pass（若转 Encaps 主线）

---

## 5. 家里 smoke（拉代码后）

```bash
cd examples/stable/stable-mlkem-f203-pke-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh

cd ../../ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec
bash run.sh -r sim -v Ascend910B4
```

---

## 6. 索引

| 主题 | 链接 |
|------|------|
| 活跃探针 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| stable / exp | [`examples/stable/INDEX.md`](examples/stable/INDEX.md) |
| KeyGen 原理 | [`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md) |
| NTT 向量 | [`docs/notes/MLKEM-NTT-向量与标量实现指南.md`](docs/notes/MLKEM-NTT-向量与标量实现指南.md) |
| 自包含约束 | [`docs/engineering/用例自包含与设备全链约束.md`](docs/engineering/用例自包含与设备全链约束.md) |
