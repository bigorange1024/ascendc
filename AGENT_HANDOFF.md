# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**512 W4+glue 完成**；glue-c η1 补缺；只推 research）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（**只推 research，勿自动合 main**；PR [#15](https://github.com/bigorange1024/ascendc/pull/15)） |
| **512** | **P0–P3 全绿**；W4 incubating E13–E21ct 全绿；**glue（AscendC RT + liboqs-512）全绿** |
| **授权** | `$规格说明书$` + `【预研代码】` |
| **用语** | **缺项** / **补缺** |

### glue-c 根因与补缺

| 项 | 说明 |
|----|------|
| **缺项** | Encrypt `r` 误用 η2（5 行全 CBD2）→ liboqs Encaps `c` 不对、`K` 仍对齐 |
| **补缺** | `r←η1=3`（PRF 192B）、`e₁‖e₂←η2=2`（PRF 128B；GM 行 stride 192） |
| **证据** | `USE_LIBOQS=1` CPU+SIM：`c`/`K`/`ek`/`dk`/accept/reject **max=0** |

### 关键 SIM tick（910B4，glue-c 后）

| ID | tick |
|----|------|
| D14 / E14 Encrypt | **365995** / **366129** |
| E20 Encaps | **427927** |

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| stable-512 | **禁**（须用户 `#交付#` 后从活跃 exp 复制晋级） |
| 可选 | D14/D20 STATUS 全量重登；教材/计划勾选 P3 退出项 |

---

## ★ 勿做

- **自动合 main**  
- 建 stable-512；frozen 抄码；零垫；擅自改已锁参数
