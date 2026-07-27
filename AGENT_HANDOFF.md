# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**512 W0 全绿 + W1 B4 绿**；下一刀 B5/B6）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（领先 `main`；PR [#15](https://github.com/bigorange1024/ascendc/pull/15)） |
| **768** | incubating + glue 有条件完成；禁 stable-768 |
| **512** | **P0+P1 完成**；**W0 全绿**（B1/B2/B3a/B3b）；**W1 B4 SampleNTT 2×2 绿** |
| **授权** | `$规格说明书$` + `【预研代码】`（继续有效） |
| **用语** | **缺项** / **补缺**；禁「洞 / 补洞」 |
| **当日纪要** | [`qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md`](qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md) |

### W0/W1 SIM tick（910B4）

| ID | 探针 | tick（摘录） |
|----|------|-------------|
| B1 | compress/decompress d=4/10 | c 3156/3442；dec 3236/3317 |
| B2 | byteencode/decode + encode12 | enc4 5407；dec4 9340；enc10 6629；dec10 6561；enc12 17613 |
| B3a | CBD η=2 polyvec4 | **11377** |
| B3b | CBD η=3 polyvec4 | **13566** |
| B4 | SampleNTT 2×2 / k2 | **80235** |

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| **T512 W1** | B5 stage123 polyvec4 · B6 Multiply/Inner |
| W2/W3 | PKE/KEM device |
| W4 | **先** customspec 再 `exp-…-k2` |

---

## ★ Smoke

```bash
bash scripts/check_mlkem512_sizes.sh
cd ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg7-sample-ntt-k2 && bash run.sh -r cpu -v Ascend910B4
```

---

## ★ 勿做

- 建 stable-512；frozen 抄码；零垫  
- 无 customspec 改 `examples/incubating/ml-kem/ml-kem-512/`  
- 擅自改已锁参数；另造用语
