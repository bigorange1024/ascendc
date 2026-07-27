# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**512 P2 W0–W3 全绿**；只推 research；下一刀 W4 customspec）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（**只推 research，勿自动合 main**；PR [#15](https://github.com/bigorange1024/ascendc/pull/15)） |
| **512** | **P0+P1**；**P2 全绿**（W0–W3：B1–B6 + D13–D15 + D19–D21ct） |
| **授权** | `$规格说明书$` + `【预研代码】` |
| **用语** | **缺项** / **补缺** |

### W3 SIM tick（910B4，摘录）

| ID | tick |
|----|------|
| D19 KeyGen | **320247** |
| D20 Encaps | **394978** |
| D21 Decaps accept/reject | **571206** / **570547** |
| D21ct accept/reject | **570707** / **571369** |

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| **T512 W4** | **先**写各 `exp-…-k2-实现方案-customspec.tex`，再按 customspec 写 incubating |
| glue | AscendC RT + **liboqs-512 KAT** |
| stable | 禁（须 `#交付#`） |

---

## ★ 勿做

- **自动合 main**  
- 无 customspec 改 `examples/incubating/ml-kem/ml-kem-512/`  
- 建 stable-512；frozen 抄码；零垫；擅自改已锁参数
