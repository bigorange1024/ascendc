# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**512 W0–W2 全绿 + W3 D20/D21/D21ct**；只推 research；D19 未纳入本提交）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（**只推 research，勿自动合 main**；PR [#15](https://github.com/bigorange1024/ascendc/pull/15)） |
| **512** | **P0+P1**；**W0+W1+W2 全绿**（B1–B6，D13–D15）；**W3 D20/D21/D21ct PASS**（D19 未纳入本提交） |
| **授权** | `$规格说明书$` + `【预研代码】` |
| **用语** | **缺项** / **补缺** |

### W2 SIM tick（910B4）

| ID | 探针 | tick | I/O |
|----|------|------|-----|
| D13 | KeyGen device | **230102** | ek 800 / dk 768 |
| D14 | Encrypt device | **338153** | c 768 |
| D15 | Decrypt device | **168975** | m 32 |

### W3 SIM tick（910B4）

| ID | 探针 | tick | I/O |
|----|------|------|-----|
| D20 | Encaps device | **394978** | c 768 / K 32 |
| D21 | Decaps device | accept **571206** / reject **570547** | dk 1632 + c 768 → K 32 |
| D21ct | Decaps CT device | accept **570707** / reject **571369** | dk 1632 + c 768 → K 32 |

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| **T512 W3** | 剩余 D19 KEM KeyGen device；D20/D21/D21ct 已绿 |
| **T512 W4** | **先** customspec 再 `exp-…-k2` |
| glue | AscendC RT + liboqs-512 KAT |

---

## ★ 勿做

- **自动合 main**  
- 建 stable-512；frozen 抄码；零垫；无 customspec 改 incubating  
- 擅自改已锁参数
