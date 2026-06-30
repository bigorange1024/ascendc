# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-06-30（晚 · 原探针 G5 PASS · 家里 2launch 探针冻结）

---

## ★ SIM 测试通过声明（2026-06-30）

**结论：`fix-f203-alg14-pke-encrypt-correctness-k4` SIM 路径完整测试通过**（507000 病根治愈，单 ACL session + `at_r5` + 全 device G4）。

| 项 | 内容 |
|----|------|
| 日期 | 2026-06-30 13:58（UTC+8）|
| 命令 | `bash run.sh -r sim -v Ascend910B4`（默认含 c.bin 对拍） |
| 工作目录 | `ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/` |
| 退出码 | **0** |
| 关键日志 | `[verify_gate] G3 u_hat + tr_hat PASS` / `[verify] PASS max=0 (1568 bytes)` |
| `Total tick`（CAModel）| **922441**（prep..pack 全 device）|
| `aclrtLaunchKernel` 返回 `507000` | **无任何一次** |

详 [`STATUS.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) · [`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md)

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull`（**主代码源**是 GitHub，不是 `backup/`） |
| 2 | 读本文件 §1，**尤其 §Encrypt** |
| 3 | [`README.md`](README.md) → [`qa/TODO.md`](qa/TODO.md) → [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 活跃探针以 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) 为准；**禁止**从 `frozen/` 抄码 |

---

## 1. 当前真相（2026-06-30 晚）

### Encrypt Alg.14（**G5 双模式 PASS — 唯一活跃探针**）

| 路径 | [`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) |
|------|------|
| **验收** | 默认 `bash run.sh` → CPU+SIM **c.bin max=0** 1568B |
| **G3** | **`at_r5` 合并核**（kP=5）；旧 G3 四核 → `compute/frozen/` |
| **Gate** | **G5 唯一生产**；G0–G4 过渡 → `frozen-gates/FROZEN.md` |
| **已关闭** | 家里分叉 [`frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/`](ascendc-tests/frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/)（`27cc93b`，**办公室未复验**） |

```bash
cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

### SIM 单 session 两大病根（已吸收到原探针 G5）

| 病根 | 解法 |
|------|------|
| R1：AIV `func_key ≥ 5` → 507000 | SIM AIV-only ≤5；`at_r5` 合并核；decode/pack MIX 占位 |
| R2：D2H 前缺 `aclrtSynchronizeStream` | matM 打包前显式 sync |

**纪要**：[`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](qa/2026-06/2026-06-30-funckey-507000本地独立验证.md) · 家里原始讨论 [`2026-06-30-Encrypt单session重建…`](qa/2026-06/2026-06-30-Encrypt单session重建与SIM-funckey5-507000病根.md)（探针已冻结）

### KeyGen（k=4，生产路径）

| 角色 | 路径 | 状态 |
|------|------|------|
| **stable 交付** | [`examples/stable/stable-mlkem-f203-pke-keygen-k4/`](examples/stable/stable-mlkem-f203-pke-keygen-k4/) | CPU/SIM/KAT ✅；SIM **542393** tick |
| **探针** | [`ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/`](ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) | 同上（prep **双 AIV 并行 Â**） |
| **incubating 副本** | [`examples/incubating/exp-mlkem-f203-pke-keygen-k4/`](examples/incubating/exp-mlkem-f203-pke-keygen-k4/) | 保留；验收以 **stable** 为准 |
| **旧 pass（串行 Â）** | [`ascendc-tests/frozen/frozen-fix-f203-alg13-device-keygen-k4/`](ascendc-tests/frozen/frozen-fix-f203-alg13-device-keygen-k4/) | **已关闭**；只读 `FROZEN.md` |

**定稿**：[`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)

### ML-KEM NTT / 2s1e 向量基线

| 路径 | 说明 |
|------|------|
| [`ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/`](ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) | Alg.13 行 16–20 全链；SIM **77958** tick |
| [`ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/`](ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) | 8-poly NTT/INTT；NTT **30347** / INTT **30340** |

### 运行约定（默认，勿手动 export）

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh             # KeyGen：CPU×10 + SIM×1
```

---

## 2. GitHub 同步

**推荐**：`git pull origin main`

### 应上传（track）

`ascendc-tests/**`（含 `frozen/`）、`docs/**`、`qa/**`、`AGENT_HANDOFF.md` 等 — 见历史 §2 表。

---

## 3. 建议下一步（见 [`qa/TODO.md`](qa/TODO.md)）

| 优先级 | ID | 事项 |
|--------|-----|------|
| 1 | T13b | vec-k4-v3 |
| 2 | T11 | 2s1e exp → stable |
| — | T14 | Encrypt G5 ✅；后继 stable 晋级 / KAT |

---

## 4. smoke（拉代码后）

```bash
cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

cd ../../examples/stable/stable-mlkem-f203-pke-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh
```

---

## 5. 索引

| 主题 | 链接 |
|------|------|
| Encrypt STATUS | [`fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) |
| 2launch 冻结 | [`frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/FROZEN.md`](ascendc-tests/frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/FROZEN.md) |
| 活跃探针 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
