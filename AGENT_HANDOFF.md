# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-06-30（晚 · 原探针 G5 PASS · 家里 2launch 探针冻结）

---

## ★ SIM 测试通过声明（2026-06-30）

**结论：`fix-f203-alg14-pke-encrypt-correctness-k4` SIM 路径首次完整测试通过**（507000 病根永久治愈，单 ACL session + `at_r5` 合并核）。

| 项 | 内容 |
|----|------|
| 日期 | 2026-06-30 12:58（UTC+8）|
| 命令 | `bash run.sh -r sim -v Ascend910B4`（默认含 c.bin 对拍） |
| 工作目录 | `ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/` |
| 退出码 | **0** |
| 关键日志 | `[verify_gate] G3 u_hat + tr_hat PASS` / `[verify] PASS max=0 (1568 bytes)` / `[SUCCESS] fix-f203-alg14-pke-encrypt-correctness-k4 gate=G5 (sim) ENCRYPT_VERIFY=1` |
| `Total tick`（CAModel）| **922441**（prep..pack 全 device）|
| `aclrtLaunchKernel` 返回 `507000` | **无任何一次** |
| host binary `nm` 残留 `g3_linear/at_r/t_dot_r` | 空 |
| CPU 孪生（同命令 `-r cpu`）| 退出码 0、`[verify] PASS max=0 (1568 bytes)` |

详 [`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) §SIM 测试通过声明 / [`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md) §6 / [`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](qa/2026-06/2026-06-30-funckey-507000本地独立验证.md) §8。

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

## 1. 当前真相（2026-06-30 午）

### Encrypt Alg.14（**G5 双模式 PASS — 唯一活跃探针**）

| 路径 | [`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) |
|------|------|
| **验收** | 默认 `bash run.sh` → CPU+SIM **c.bin max=0** 1568B；详 §SIM 测试通过声明 |
| **G3** | **`at_r5` 合并核**（kP=5）；旧 G3 四核 → `compute/frozen/` |
| **Gate** | **G5 唯一生产**；G0–G4 过渡 → `frozen-gates/FROZEN.md` |
| **已关闭** | 家里分叉 [`frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/`](ascendc-tests/frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/)（`27cc93b`，**办公室未复验**） |

```bash
cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

**纪要**：[`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](qa/2026-06/2026-06-30-funckey-507000本地独立验证.md) §8–§11  
**原理沉淀**：[`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md)

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
| 1 | T13b | vec-k4-v3 |
| 2 | T11 | 2s1e exp → stable |

---

## 5. 家里 smoke（拉代码后）

```bash
# P0：Encrypt 双模式（CPU 应 PASS；SIM 应 PASS 且无 507000）
cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

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
