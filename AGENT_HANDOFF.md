# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**512 P0+P1 已落**；下一刀 W0 / B3b CBD‑η3）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（与 `main` 对齐后推进 512） |
| **768** | incubating + glue **有条件完成**；真 liboqs 交叉已绿；禁 stable-768 |
| **512** | **P0+P1 完成**：目录壳、sizes、registry×6、P1 表；`sample_poly_cbd3` 已入 shared |
| **授权** | `$规格说明书$` + `【预研代码】`（2026-07-27） |
| **用语** | **缺项** / **补缺** / **补缺图**；禁「洞 / 补洞」 |
| **当日纪要** | [`qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md`](qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md) |

### 512 已锁默认

S-1；T-B2 polyvec4；`-k2`；CBD‑η3 缺项；D19–D21+CT；PKE exp；liboqs-512 交叉必达；禁零垫 / 禁 stable-512。

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| **T512 W0** | B3b `pass-fix-f203-alg8-cbd-eta3-k2`（优先）→ B1/B2/B3a |
| W4 | 先写 `*-customspec.*` 再动 `exp-…-k2` |
| stable | 仅 `#交付#` |

---

## ★ Smoke

```bash
bash scripts/check_mlkem512_sizes.sh
bash scripts/smoke_liboqs_kem_params.sh
test -f docs/specs/fips203-mlkem512-p1-gap-and-cases.md
test -d ascendc-tests/ml-kem/ml-kem-512
test ! -d examples/stable/ml-kem/ml-kem-512
```

---

## ★ 勿做

- 建 stable-512；从 `**/frozen/**` 抄实现；零垫  
- 无 customspec 改 `examples/incubating/ml-kem/ml-kem-512/`  
- 擅自改已锁参数；另造方法论用语（须用 **缺项/补缺**）
