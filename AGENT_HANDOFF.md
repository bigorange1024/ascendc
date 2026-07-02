# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-07-02（**Alg.21 Decaps 首版 CPU PASS + SIM 合法路径 workaround PASS**；单 session SIM 待修 · 详 [`qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md`](qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md) §7）

---

## ★ 下一任务（家里 Agent · P0）

### 1. Alg.21 Decaps — 修 SIM 单 session + 拒绝路径

**现状**：合法密文路径 **CPU PASS（单 session + 设备 FO）**、**SIM PASS（两段 session workaround）**。SIM 单 session 与拒绝路径**未过**。

| 项 | 内容 |
|----|------|
| **目录** | [`ascendc-tests/fix-f203-alg21-kem-decaps-k4/`](ascendc-tests/fix-f203-alg21-kem-decaps-k4/) |
| **老三样** | 方案 [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg21-kem-decaps-k4/INTEGRATION_PLAN.md) §11 · qa §7 · note [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |
| **根因（已确认）** | SIM 单 session 内 Decrypt→Encrypt，`m'/K'/coins` dump **max=0** 但重加密 `c' max=244` → FO 走 `J(z‖c)` → `K` 错。**非**种子/输入问题；同组 `m'/coins` 单独 alg14 G5 SIM `c' max=0`。 |
| **workaround（当前 SIM）** | Phase-D+G → `aclFinalize` → fresh `run_g5_sim_full` 重加密 → host `memcmp(c,c')` 取 `K'`（**未跑设备 FO / 拒绝路径**） |

**待办**：

1. 定位 CAModel/ACL 超长单 session 状态污染点，恢复 §4.1 单 session D→G→E→FO（SIM）。
2. SIM **拒绝路径**（篡改 `c` 一字节 → `K` 应 `= J(z‖c)`；建议 `KEM_DECAPS_VERIFY=2`）。
3. `nm build/.../device_aiv.o` **func_key 分库审计**（decrypt / encrypt 分库；encrypt 侧同时含原 pack 与 dec pack）。
4. 扩 `scripts/liboqs_kem_vs_ascendc.sh` **decaps** 段。

```bash
# 前置（各跑一次即可；后续 SKIP_REBUILD）
cd ascendc-tests/fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg20-kem-encaps-k4 && KEM_ENCAPS_SKIP_REBUILD=1 bash run.sh -r cpu -v Ascend910B4

# Decaps 复现（CPU 单 session 全链；SIM 当前 workaround）
cd ../fix-f203-alg21-kem-decaps-k4
KEM_DECAPS_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
KEM_DECAPS_SKIP_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim -v Ascend910B4
```

**WSL 约束**：默认 `CMAKE_BUILD_JOBS=2`；`KERNEL_COMPUTE_BUDGET_SEC=1800`（SIM 长）；勿 `cmake -j` 满核；勿并行多 SIM（decrypt+encrypt 两段库编译体量大）。

---

### 2. Alg.20 Encaps SIM 复验 + alg14 run.sh 对齐（承接，未完）

| 项 | 状态 |
|----|------|
| **T7a** Alg.20 | **CPU+SIM PASS**（tick ~103 万）；`scripts/liboqs_kem_vs_ascendc.sh` **encaps 段仍待扩** |
| **T7b** alg14 `run.sh` | **待开工**：对齐 alg20（`ENCRYPT_SKIP_REBUILD`/`FORCE_REBUILD`/`CMAKE_BUILD_JOBS=2`；去默认 `rm -rf build out`）；G5 代码已仅输出 `c.bin` |

```bash
cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4   # 改 run.sh 后回归
SIM_DIRECT=1 ENCRYPT_VERIFY=1 bash run.sh -r sim -v Ascend910B4
```

---

## ★ 已完成（本推送含）

### Alg.21 Decaps（T7c 有条件 PASS）

| 项 | 内容 |
|----|------|
| **架构** | vendor alg15 Decrypt G4 + alg14 Encrypt G5 + `kem/` K1 `G(m'‖h)` + K2 FO；decrypt/encrypt **分库** |
| **I/O** | `dk_kem`(3168)+`c`(1568) → **`K.bin`(32) only**；输入复制 alg19/20（`SEED_D=20260619`） |
| **CPU** | **PASS** `K max=0`（单 session + 设备 FO） |
| **SIM** | **PASS** `K max=0`（两段 session workaround；tick D~534k + E~899k） |

### Alg.19 KeyGen（T6 PASS） / Alg.20 Encaps（T7a PASS）

| 探针 | 验收 |
|------|------|
| [`fix-f203-alg19-kem-keygen-k4`](ascendc-tests/fix-f203-alg19-kem-keygen-k4/) | CPU+SIM+liboqs **max=0** · SIM **742558** |
| [`fix-f203-alg20-kem-encaps-k4`](ascendc-tests/fix-f203-alg20-kem-encaps-k4/) | CPU+SIM **c/K max=0** · tick **~1029406** |

---

## liboqs / PKE 回归（smoke）

```bash
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4
bash scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4   # keygen；encaps/decaps 段待扩

cd ascendc-tests/fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg14-pke-encrypt-correctness-k4 && ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg15-pke-decrypt-correctness-k4 && bash run.sh -r cpu -v Ascend910B4
```

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull` |
| 2 | 读本文件 **§下一任务** |
| 3 | [`qa/TODO.md`](qa/TODO.md) · [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 活跃探针 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| 5 | **禁止**从 `frozen/` 抄码 · **禁止**落盘非算法输出 · **SIM workaround 不是新基线** |

---

## 索引

| 主题 | 链接 |
|------|------|
| 当日纪要 §7 | [`qa/2026-07/2026-07-02-…md`](qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md) |
| Alg.21 方案 | [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg21-kem-decaps-k4/INTEGRATION_PLAN.md) |
| Alg.21 STATUS | [`fix-f203-alg21-kem-decaps-k4/STATUS.md`](ascendc-tests/fix-f203-alg21-kem-decaps-k4/STATUS.md) |
| Alg.21 note | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |
| 遗留总表 | [`qa/TODO.md`](qa/TODO.md) |
