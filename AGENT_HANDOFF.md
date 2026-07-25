# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-25（教材第7章成文；liboqs RT main/CT 全绿；交付树工程回灌）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（`origin/main` 已合入；勿开 `cursor/*` 旁支） |
| **KEM 六算子 stable** | KeyGen / Encaps / Decaps **均已定型**（交付 Decaps **无 `-ct`**）；`scripts/` 默认指 **无 `-ct`** stable |
| **Decaps 交付工程回灌** | 2026-07-25：`verify || exit $?` + 注释/`M_FILE` 文档；**未**改 `decaps_1session` |
| **Decaps 交付树**（main；`scripts/` 默认） | device [`pass-fix-…-decaps-device-k4`](ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/) · incubating [`exp-…-kem-decaps-k4`](examples/incubating/exp-fips203-mlkem-kem-decaps-k4/) · stable [`stable-…-kem-decaps-k4`](examples/stable/stable-fips203-mlkem-kem-decaps-k4/)（**T19i SIM 3**；tick **1050620**） |
| **Decaps CT 树**（本专题；仅 `research/formal-lang-dag`） | device / exp / stable `*-decaps-*-ct-k4`（第7章 CT / 五指标；SIM `decaps_2session`）；合法 tick 已入 [`qa/active_sim_regress_summary.md`](qa/active_sim_regress_summary.md)（约 **1.05M**） |
| **形式方法教材** | [`docs/research/`](docs/research/) 第6–7章；第7章 CT 实验引用 **`-ct`** 树 |
| **Alg.19/20/21 correctness** | **已冻结** → `ascendc-tests/frozen/frozen-fix-…-*-correctness-k4/`（只读 FROZEN.md；**禁翻源码**） |
| **办公室回归** | [`scripts/stable_kem_liboqs_roundtrip.sh`](scripts/stable_kem_liboqs_roundtrip.sh)：**无 `-ct`** stable 三件套；urandom→liboqs→同字节喂 AscendC；**CPU×1 + SIM×1** |
| **五指标对照** | [`docs/research/2026-07-24-Decaps-correctness与CT五指标对照.md`](docs/research/2026-07-24-Decaps-correctness与CT五指标对照.md)（**`-ct`** 专题） |

---

## ★ 下一刀（P0）

按用户指定。常见候选项：

| 优先级 | 事项 |
|--------|------|
| **P1** | **T23** 多 AI Core 并行 stable（先 2 Core） |
| **P1** | **T2-npu** / **T21**（SHA3hp 调研） |
| 专题 | 教材细表 / **NPU** 冒烟（有卡时；交付树 **无 `-ct`**） |

---

## ★ Smoke

**办公室 KEM↔liboqs（交付默认，无 `-ct`）**：

```bash
set -o pipefail
bash scripts/stable_kem_liboqs_roundtrip.sh 2>&1 | tee /tmp/stable_kem_rt.log
echo EXIT:$?
```

单算子（交付 stable Decaps）：

```bash
cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

**CT 专题（本分支；勿与交付默认混用）**：

```bash
cd ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-ct-k4
# 或 examples/stable/stable-fips203-mlkem-kem-decaps-ct-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 注意 | 说明 |
|------|------|
| **验收口径** | CPU 全绿 + SIM 全绿才算端到端通过 |
| **WSL 偶发** | 连续 SIM 后 Decaps Phase-D 曾见 `tcache` abort；公司+家里对照矩阵均未复现 → 归类 CAModel/glibc 偶发；**未**改 roundtrip/stable |
| **复验** | 遇上述失败：对 Decaps 目录单独 `bash run.sh -r sim`；绿则记环境偶发 |
| **定点** | `KEM_SEED_HEX=… M_HEX=… bash scripts/stable_kem_liboqs_roundtrip.sh` |

---

## ★ 勿做

- 从 `**/frozen/**` 抄码；未 `#…#` 改 stable 实现
- WSL 上 `run.sh -r npu`
- Decaps 全链只传 `DK_KEM_SRC` 不传 `EK_KEM_SRC`（stash ek → FO 假拒）
- **另起分支** / **无指令擅自 commit·push**（见 Rule「Git 分支 / 提交 / 推送」）
- 把连续 SIM 的 CAModel `tcache` 偶发当成算法回归去改稳定算子
- 写「定型交付 / scripts 默认」时误指 **`-ct`** 树；写第7章 CT / 五指标时误指**无 `-ct`** 交付树
