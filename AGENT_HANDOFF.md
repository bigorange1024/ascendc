# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-20（WSL 连续 SIM 偶发记入 · roundtrip Smoke · **main**）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **KEM 三件套 stable** | KeyGen / Encaps / Decaps **均已定型**；六算子齐；**T19i SIM 3** |
| **Alg.19/20/21 correctness** | **已冻结** → `ascendc-tests/frozen/frozen-probe-…-*-correctness-k4/`（只读 FROZEN.md；**禁翻源码**） |
| **办公室回归入口** | [`scripts/stable_kem_liboqs_roundtrip.sh`](scripts/stable_kem_liboqs_roundtrip.sh)：**先** liboqs `urandom` → **再**同字节喂 AscendC；**CPU×1 + SIM×1** |
| **证据** | Cloud 端到端全绿：`output/stable_kem_liboqs_rt/20260720_102426_216972/`；WSL 见下 Smoke 偶发说明 |
| **T6 / T7a / T7c / T19i** | **关闭** |
| **T23** | **打开**：多 AI Core∥stable |

---

## ★ 下一刀（P0）

| 优先级 | 事项 |
|--------|------|
| **P1** | **T23** 多 AI Core 并行 stable（先 2 Core） |
| **P1** | **T2-npu** / **T21** |

---

## ★ Smoke

```bash
# 外层 tee 必须 pipefail，否则失败仍可能显示 exit 0
set -o pipefail
bash scripts/stable_kem_liboqs_roundtrip.sh 2>&1 | tee /tmp/stable_kem_rt.log
echo EXIT:$?
```

| 注意 | 说明 |
|------|------|
| **验收口径** | CPU 全绿 + SIM 全绿才算端到端通过 |
| **WSL 偶发** | 连续 SIM（KeyGen→Encaps→立刻 Decaps）可能 Phase-D `tcache … unaligned` core dump；**同 fixture 单独** Decaps SIM 往往仍绿 → **CAModel/堆偶发**，非稳定算法错（见当日 qa） |
| **复验** | 遇上述失败：对 Decaps 目录单独 `bash run.sh -r sim -v Ascend910B4`（同次 ek/dk/c）；绿则记环境偶发 |
| **定点** | `KEM_SEED_HEX=… M_HEX=… bash scripts/stable_kem_liboqs_roundtrip.sh` |

单算子冒烟：

```bash
cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

---

## ★ 勿做

- 从 `**/frozen/**` 抄码；未 `#…#` 改 stable 实现
- WSL 上 `run.sh -r npu`
- Decaps 全链只传 `DK_KEM_SRC` 不传 `EK_KEM_SRC`（stash ek → FO 假拒）
- **另起分支** / **无指令擅自 commit·push**（见 Rule「Git 分支 / 提交 / 推送」）
- 把连续 SIM 的 CAModel `tcache` 偶发当成算法回归去改稳定算子
