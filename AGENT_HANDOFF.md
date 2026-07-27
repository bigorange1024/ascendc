# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**512 W4+glue 有条件完成**；只推 research）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（**只推 research，勿自动合 main**；PR [#15](https://github.com/bigorange1024/ascendc/pull/15)） |
| **512** | **P0–P2 全绿**；**P3 W4 incubating E13–E21ct 全绿**；**glue 有条件完成** |
| **授权** | `$规格说明书$` + `【预研代码】` |
| **用语** | **缺项** / **补缺** |

### W4 SIM tick（910B4，摘录）

| ID | tick |
|----|------|
| E13 KeyGen | **230036** |
| E14 Encrypt | **338121** |
| E15 Decrypt | **168783** |
| E19 KEM KeyGen | **319957** |
| E20 Encaps | **397538** |
| E21 accept D+E | **163062** + **406837** |
| E21ct accept D+E | **163062** + **405028** |

### glue

| 项 | 结果 |
|----|------|
| AscendC RT CPU + SIM | **PASS**（[`scripts/exp_kem512_liboqs_roundtrip.sh`](scripts/exp_kem512_liboqs_roundtrip.sh)） |
| liboqs-512 | KeyGen **PASS**；Encaps **K PASS / c 未对齐**（下一刀） |

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| **T512 glue-c** | 查清 Encaps `c` 相对 liboqs-512 的缺项（K 已对齐） |
| stable | 禁（须 `#交付#`） |

---

## ★ 勿做

- **自动合 main**  
- 建 stable-512；frozen 抄码；零垫；擅自改已锁参数
