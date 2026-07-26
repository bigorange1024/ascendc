# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-26（WSL P0 分类复测：本轮 main+CT CPU×3+SIM×1 均绿；早先 CT 连续 SIM 偶发仍记宿主机）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`main`**（与 `research/formal-lang-dag` tip 曾对齐；WSL 测交付树请拉 **`main`**） |
| **KEM 六算子 stable** | KeyGen / Encaps / Decaps **均已定型**（交付 Decaps **无 `-ct`**）；`scripts/` 默认指 **无 `-ct`** stable |
| **Decaps 交付树**（`scripts/` 默认） | `stable-…-kem-decaps-k4`（**T19i**；SIM 默认 `decaps_1session`） |
| **Decaps CT 树**（教材/对照） | `*-decaps-*-ct-k4`（SIM 默认 `decaps_2session`）；**非**办公室默认 |
| **已知偶发** | 连续 SIM 时 Decaps Phase-D 曾见 `tcache` / segfault；单独 Decaps SIM 常绿 → CAModel/glibc 宿主机偶发 |
| **WSL P0（今日）** | 交付+CT 均 `CPU×3+SIM×1` **EXIT=0**（fixture `…175815_1481` / `…184309_6528`）；早先 CT 连续红仍记偶发，见当日 qa |

---

## ★ 下一刀（P0）— WSL Agent 照做

**目标**：复现并**分类** Decaps「有时失败」（交付树 vs CT；连续 SIM vs 单独 Decaps）。  
**约束**：只跑测 + 写当日 `qa/`；**禁止**为消偶发改 `stable_kem_liboqs_roundtrip.sh` / stable 核；勿并行多路 SIM。

### 1) 准备

```bash
git pull origin main
set -o pipefail
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
mkdir -p /tmp/wsl_kem_rt
```

### 2) 基线：交付树（无 `-ct`）

```bash
CPU_TRIALS=3 SIM_TRIALS=1 bash scripts/stable_kem_liboqs_roundtrip.sh \
  2>&1 | tee /tmp/wsl_kem_rt/main_rt.log
echo EXIT:$?
```

记录：EXIT、fixture（`output/stable_kem_liboqs_rt/<ts>/`）、失败形态：

| 形态 | 特征 |
|------|------|
| 宿主机崩 | `tcache_*` / `Aborted` / `signal 11` / EXIT 139；常无 Decaps `Total tick` |
| 假拒 / 对拍 | `K(decaps) max≠0` 或 verify 非零（查是否缺 `M_FILE`） |

### 3) 若 2) 的 SIM Decaps 失败：同 fixture 单独复验

用失败那次 fixture 里已有 KeyGen/Encaps 输出，**只**跑交付 Decaps（勿整链重跑）：

```bash
cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
# 按该目录 STATUS / roundtrip 脚本惯例接好 EK_KEM_SRC DK_KEM_SRC M_FILE 等（与失败 trial 同字节）
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4 2>&1 | tee /tmp/wsl_kem_rt/main_decaps_solo.log
echo EXIT:$?
```

| 单独结果 | 记法 |
|----------|------|
| 绿 | **连续 SIM 宿主机偶发**；非算法回归 |
| 红 | 贴 verify/`max` 与 Phase-D/E 日志尾；升级为待查接线/污染 |

### 4) CT 对照（方法论树；非默认）

```bash
DECAPS_DIR=$PWD/examples/stable/stable-fips203-mlkem-kem-decaps-ct-k4 \
  CPU_TRIALS=3 SIM_TRIALS=1 bash scripts/stable_kem_liboqs_roundtrip.sh \
  2>&1 | tee /tmp/wsl_kem_rt/ct_rt.log
echo EXIT:$?
```

若连续 SIM segfault：对 CT 树重复步骤 3（目录换成 `…-decaps-ct-k4`）。

### 5) 回写

追加到**当日** `qa/YYYY-MM/YYYY-MM-DD-….md`（无则新建；同日只一篇）：

- 命令、EXIT、fixture 路径
- 失败形态分类 + 单独 Decaps 复验结果
- 同步一行到 `qa/INDEX.md` / 当月 `qa/YYYY-MM/INDEX.md`
- **commit + push `main`**（仅文档/纪要）

---

## ★ Smoke（日常，非本次 P0）

```bash
set -o pipefail
bash scripts/stable_kem_liboqs_roundtrip.sh 2>&1 | tee /tmp/stable_kem_rt.log
echo EXIT:$?
```

---

## ★ 勿做

- 从 `**/frozen/**` 抄码；未 `#…#` 改 stable 实现
- WSL 上 `run.sh -r npu`
- 把连续 SIM 的 CAModel `tcache`/segfault 偶发当成算法回归去改稳定算子
- 写「定型交付 / scripts 默认」时误指 **`-ct`** 树
- 并行多路 SIM；外层 `bash … \| tee` 忘记 `set -o pipefail`（会掩盖非零 EXIT）
