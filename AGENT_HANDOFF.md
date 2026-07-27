# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**768 有条件完成至 incubating + glue**；**教材第8章**已成文；正确性/性能已汇报）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`** · PR [#12](https://github.com/bigorange1024/ascendc/pull/12) |
| **768 参数卡** | [`docs/specs/fips203-mlkem768-parameter-card.md`](docs/specs/fips203-mlkem768-parameter-card.md) **已锁**（§3.1–§3.3） |
| **探针 W0–W3** | **全绿**（CPU + `SIM_DIRECT=1` sim；tick 见 [`qa/active_sim_regress_summary.md`](qa/active_sim_regress_summary.md)） |
| **incubating-768** | **W4 + glue 已完成**：E13–E15、E19–E21ct 均有 customspec + CPU/SIM；registry 已补；[`scripts/exp_kem768_liboqs_roundtrip.sh`](scripts/exp_kem768_liboqs_roundtrip.sh) **AscendC-only** CPU×1+SIM×1 PASS |
| **stable-768** | **本阶段不建**（须 `#交付#`） |
| **当日纪要** | [`qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md`](qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md) |
| **教材** | 第8章（`sec:mlkem768`）已写入形式方法教材草案 PDF |

### 用户决议（摘要）

T-B polyvec6 + Â prep；禁零垫；保留 D19–D21 + CT；PKE exp 必做；`-k3`；已授权 W0–W4。

---

## ★ 下一刀（可选 / 非门禁）

| 项 | 说明 |
|----|------|
| **T768-post** | liboqs-768 helper、device KAT、D14↔D15 PKE RT |
| **stable-768** | 仅当用户 `#交付#` |
| 仓级 | T23 / T21 / T2-npu / 教材细表 |

---

## ★ Smoke

```bash
bash scripts/check_mlkem768_sizes.sh
SKIP_SIM=1 bash scripts/exp_kem768_liboqs_roundtrip.sh
test ! -d examples/stable/ml-kem/ml-kem-768
```

---

## ★ 勿做

- 从 `**/frozen/**` 抄实现；零垫凑 4/8  
- 无 `#交付#` 建 stable-768  
- 擅自改 §3.1–§3.3  
- 擅自改 `.cursor/rules/` / `.cursor/skills/`  
