# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**额度复盘**入 engineering；512 已合 main；工作分支 research）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（与 `main` 对齐于 `981adba` 后；PR [#15](https://github.com/bigorange1024/ascendc/pull/15) **已合**） |
| **512** | **P0–P3 有条件完成**；W4+glue（含 liboqs-512）全绿；禁 stable-512 |
| **768** | **有条件完成至 incubating**（禁 stable-768） |
| **1024 Encaps `m`** | 默认 **`os.urandom(32)`**；禁默认可全 0 |
| **用语** | **缺项** / **补缺** |
| **额度经验** | [`docs/engineering/Cloud-Agent额度与验收分层.md`](docs/engineering/Cloud-Agent额度与验收分层.md)（**不改** ML-KEM 流程） |

### glue-c（已闭环）

Encrypt `r←η1=3` / `e←η2=2`；`USE_LIBOQS=1` Encaps `c`+`K` max=0。关键 tick：D14 **365995** / E14 **366129** / E20 **427927**。

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| stable-512 / stable-768 | 须用户 `#交付#` |
| 可选 | D20 tick 重登；T23 / T21 / T2-npu |

---

## ★ 勿做

- frozen 抄码；零垫；擅自改已锁参数；默认全 0 Encaps `m`
