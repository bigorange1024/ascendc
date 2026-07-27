# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**512 W0+W1 全绿；W2 D15 PASS**；只推 research，不合 main）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（**只推 research，勿自动合 main**；PR [#15](https://github.com/bigorange1024/ascendc/pull/15)） |
| **768** | incubating + glue 有条件完成；禁 stable-768 |
| **512** | **P0+P1 完成**；**W0+W1 全绿**（B1–B6）；**W2 D15 PASS**（tick **168975**） |
| **授权** | `$规格说明书$` + `【预研代码】` |
| **用语** | **缺项** / **补缺** |

### W1 SIM tick（910B4）

| ID | 探针 | tick |
|----|------|------|
| B4 | SampleNTT 2×2 | **80235**（含 matrix PASS） |
| B5 | stage123 polyvec4 | NTT **22921** / INTT **22836** |
| B6 | Multiply + Inner | multiply **9290** / inner **12603** |

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| **T512 W2** | D15 PKE Decrypt 已绿；D13/D14 待补/并行接力（k=2 I/O D13 ek/dk、D14 c、D15 dk+c→m） |
| W3 | D19–D21ct KEM device |
| W4 | **先** customspec 再 `exp-…-k2` |

---

## ★ Smoke

```bash
bash scripts/check_mlkem512_sizes.sh
cd ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg15-pke-decrypt-device-k2
bash run.sh -r cpu -v Ascend910B4
```

---

## ★ 勿做

- **自动合 main**（用户要求：只推 research）  
- 建 stable-512；frozen 抄码；零垫；无 customspec 改 incubating  
- 擅自改已锁参数；另造用语
