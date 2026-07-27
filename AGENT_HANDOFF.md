# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**512 P0+P1 已落**；**W0 B3a CBD‑η2 + B3b CBD‑η3 CPU+SIM 绿**）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`** @ `b0c95a4` |
| **768** | incubating + glue 有条件完成；禁 stable-768 |
| **512** | **P0+P1 完成**；W0 **B3a CBD‑η2** `pass-fix-f203-alg8-cbd-eta2-k2` **CPU+SIM PASS**（tick **11377**）；**B3b CBD‑η3** `pass-fix-f203-alg8-cbd-eta3-k2` **CPU+SIM PASS**（tick **13566**） |
| **授权** | `$规格说明书$` + `【预研代码】` |
| **用语** | **缺项** / **补缺**；禁「洞 / 补洞」 |
| **当日纪要** | [`qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md`](qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md) |

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| **T512 W0** | B1 Compress · B2 ByteEncode |
| W1 | SampleNTT 2×2 · polyvec4 NTT · Multiply/Inner |
| W4 | **先** `*-customspec.*` 再 `exp-…-k2` |

---

## ★ Smoke

```bash
bash scripts/check_mlkem512_sizes.sh
cd ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg8-cbd-eta3-k2
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
cd ../pass-fix-f203-alg8-cbd-eta2-k2
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

---

## ★ 勿做

- 建 stable-512；frozen 抄码；零垫  
- 无 customspec 改 `examples/incubating/ml-kem/ml-kem-512/`  
- 擅自改已锁参数；另造用语
