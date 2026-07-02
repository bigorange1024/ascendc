# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-07-02（**Alg.20 Encaps 首版 CPU PASS** · **T7a SIM 待复验** · 详 [`qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md`](qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md) §6）

---

## ★ 下一任务（办公室 Agent · P0）

### 1. Alg.20 Encaps — SIM 复验 + liboqs encaps 段

| 项 | 内容 |
|----|------|
| **目录** | [`ascendc-tests/fix-f203-alg20-kem-encaps-k4/`](ascendc-tests/fix-f203-alg20-kem-encaps-k4/) |
| **状态** | **CPU PASS**（`c`/`K` max=0）；**SIM 待复验**（早前 tick **1029406** PASS，run.sh 重写后需再跑） |
| **pk** | 事先存在 `../fix-f203-alg19-kem-keygen-k4/output/ek_kem.bin`；**不**在 alg20 内跑 KeyGen |
| **TODO** | **T7a**（CPU ✅ · SIM ⏳ · liboqs encaps 脚本 ⏳） |

```bash
# 先确认 pk（必须已有）
ls -l ascendc-tests/fix-f203-alg19-kem-keygen-k4/output/ek_kem.bin

# CPU 冒烟（可跳过重建）
cd ascendc-tests/fix-f203-alg20-kem-encaps-k4
KEM_ENCAPS_SKIP_REBUILD=1 KEM_ENCAPS_VERIFY=1 bash run.sh -r cpu -v Ascend910B4

# SIM（单独跑，勿并行其他 SIM；~15min）
KEM_ENCAPS_SKIP_REBUILD=1 KEM_ENCAPS_VERIFY=1 bash run.sh -r sim -v Ascend910B4
```

**WSL 约束**：默认 `CMAKE_BUILD_JOBS=2`；勿 `cmake -j` 满核；勿同时跑 alg14/alg19 SIM。

---

### 2. alg14 Encrypt — `run.sh` 对齐 alg20（**代码已改，脚本未改**）

G5 生产路径已去掉中间量落盘与默认 `verify_gate`；**`run.sh` 仍每次 `rm -rf build out` + `cmake -j` 满核** — 需对齐 alg20 模式：

| 环境变量（拟增） | 含义 |
|------------------|------|
| `ENCRYPT_SKIP_REBUILD=1` | 二进制在且 RUN_MODE 未变 → 跳过 cmake |
| `ENCRYPT_FORCE_REBUILD=1` | 才清 `build/`/`out/` |
| `CMAKE_BUILD_JOBS=2` | 默认并行编译线程 |

验收仍仅 **`verify_result.py` → `c.bin` max=0**；`verify_gate.py` 已废弃（frozen G1–G4 手工回放）。

```bash
cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4   # 改 run.sh 后回归
SIM_DIRECT=1 ENCRYPT_VERIFY=1 bash run.sh -r sim -v Ascend910B4
```

---

## ★ 已完成（本推送含）

### Alg.19 KeyGen（T6 PASS）

| 项 | 内容 |
|----|------|
| **目录** | [`fix-f203-alg19-kem-keygen-k4`](ascendc-tests/fix-f203-alg19-kem-keygen-k4/) |
| **验收** | CPU+SIM+liboqs **max=0** · SIM tick **742558** |

### Alg.20 Encaps（T7a 部分）

| 项 | 内容 |
|----|------|
| **架构** | vendor Encrypt G5 + KEM 头并入 `prep_re` |
| **I/O** | `ek_kem`+`seed_d` → **`c`+`K` only**（无中间张量落盘） |
| **CPU** | **PASS** `SEED_D=20260619` |

### alg14 生产 I/O 治理（无 run.sh 优化）

- G5 SIM/CPU 仅写 **`output/c.bin`**
- 默认 **`run.sh` 不再调用 `verify_gate.py`**

---

## liboqs / PKE 回归（smoke）

```bash
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4
bash scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4   # keygen；encaps 段待扩

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
| 5 | **禁止**从 `frozen/` 抄码 · **禁止**落盘非算法输出 |

---

## 索引

| 主题 | 链接 |
|------|------|
| 当日纪要 §6 | [`qa/2026-07/2026-07-02-…md`](qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md) |
| Alg.20 STATUS | [`fix-f203-alg20-kem-encaps-k4/STATUS.md`](ascendc-tests/fix-f203-alg20-kem-encaps-k4/STATUS.md) |
| Alg.20 方案 | [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg20-kem-encaps-k4/INTEGRATION_PLAN.md) |
| 遗留总表 | [`qa/TODO.md`](qa/TODO.md) |
