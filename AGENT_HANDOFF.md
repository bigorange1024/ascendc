# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-07（**Alg.14 Encrypt prep+compute 晋级 `pass-`** · compute kP=5/v/decode 完成 · 分平台 pass 表述定案）

---

## ★ 当前真相（Encrypt prep + compute，2026-07-07）

### Alg.14 Encrypt prep — **完成（CPU+SIM）**

探针：[`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4`](ascendc-tests/pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/)

| 项 | 内容 |
|----|------|
| 范围 | 行 3–7 `a_hat` + 行 8–15 `re`（r/e₁/e₂） |
| Launch | 单 launch `f203_encrypt_prep` |
| 验收 | `a_hat` / `re` max=0；CPU + SIM |
| SIM tick | ~470502 |

### Alg.14 Encrypt compute — **SIM 完成 / CPU 部分**

探针：[`pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4`](ascendc-tests/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/)

| 模式 | Launch | 对拍 | 判定 |
|------|--------|------|------|
| **SIM 默认** | 单 launch `f203_encrypt_l18_l19` | y_hat, u_ntt, u_tr, u, v | **完成**（行 2/18/19/21） |
| **CPU** | 3 launch（ntt_y→at_jp→intt_e1） | y_hat, u_ntt, u | **部分对照**（tikicpu 不得融合） |
| SIM 调试 | `ASCENDC_SIM_HOST_MODE=phased_launch` | 同 CPU | 分段调试 |

**定案**：û/uTr **驻留 UB** → INTT `ProcessFromLocal`；kP=5 pad→8；MIX GATE **4→8**；INTT flag **1/3**。

**未做**：prep + compute **GM 级拼接**（目标 2 launch Encrypt 核心）；μ / Compress `c`。

### KEM 三分项（维持 07-03）

KeyGen / Encaps / Decaps 分项 kat **CPU×10+SIM×1 PASS**；Decaps SIM 默认 **2-session**。

---

## ★ 下一任务（P0）

1. **prep + compute 拼接**：`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4` 输出直连 compute 输入 → 目标 **2 launch** Encrypt 核心（见 compute `INTEGRATION_PLAN` §8）。
2. **Alg.21 Decaps 单 session SIM 真修**（2-session 已是保底）。
3. **T7b alg14 correctness `run.sh` 资源友好化**。
4. **NPU 实机**：KEM + PKE 均未测。

---

## 验收命令（smoke）

```bash
# Encrypt prep（双模式全 pass）
cd ascendc-tests/pass-fix-f203-alg14-lines3-15-encrypt-prep-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# Encrypt compute（SIM 全量；CPU 部分）
cd ../pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# KEM 分项 kat（回归）
bash scripts/liboqs_kem_keygen_batch.sh
bash scripts/kem_keypair_stash_bootstrap.sh
bash scripts/liboqs_kem_encaps_batch.sh
bash scripts/liboqs_kem_decaps_batch.sh
```

**WSL 约束**：`CMAKE_BUILD_JOBS=2`；compute 单 launch 默认 `KERNEL_COMPUTE_BUDGET_SEC=600`；勿并行多 SIM。

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull` |
| 2 | 读本文件 **§当前真相 / §下一任务** |
| 3 | 当日纪要 [`qa/2026-07/2026-07-07-CPU-SIM-launch分叉约定.md`](qa/2026-07/2026-07-07-CPU-SIM-launch分叉约定.md) |
| 4 | compute 定稿 [`docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md`](docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md) |
| 5 | [`qa/TODO.md`](qa/TODO.md) T17 · [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 6 | **禁止**从 `frozen/` 抄码 · **中间态驻 UB**，GM 仅 dump 对拍 |

### 接手步骤（Encrypt 2 launch 拼接）

1. 读 prep [`STATUS.md`](ascendc-tests/pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/STATUS.md) + compute [`STATUS.md`](ascendc-tests/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/STATUS.md)。
2. compute **不得**在 CPU 上试单 launch；SIM 为生产验收面。
3. 拼接时保持各自 golden 几何；先 host 串联 smoke，再设备 GM 直连。

---

## 附录：关键路径速查

| 主题 | 路径 |
|------|------|
| prep 方案 | [`INTEGRATION_PLAN.md`](ascendc-tests/pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/INTEGRATION_PLAN.md) |
| compute 方案 | [`INTEGRATION_PLAN.md`](ascendc-tests/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/INTEGRATION_PLAN.md) |
| CPU/SIM 分叉 | [`docs/notes/AscendC-CPU与SIM实现分叉开发指南.md`](docs/notes/AscendC-CPU与SIM实现分叉开发指南.md) |
| UB 驻留原理 | [`docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md`](docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md) |
| 探针索引 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
