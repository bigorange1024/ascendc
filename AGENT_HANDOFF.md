# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**512 计划草案待锁**；768 已有条件完成至 incubating）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`** · PR [#12](https://github.com/bigorange1024/ascendc/pull/12)（已合入 `main` @ `854a6d5`；research 仍活跃） |
| **768** | incubating + glue **有条件完成**；禁 stable-768 |
| **512** | **计划 + 参数卡草案已成文，待用户锁 §决议** |
| **512 计划** | [`docs/research/MLKEM-512-从0到exp完整实现计划.md`](docs/research/MLKEM-512-从0到exp完整实现计划.md) |
| **512 参数卡** | [`docs/specs/fips203-mlkem512-parameter-card.md`](docs/specs/fips203-mlkem512-parameter-card.md) |
| **当日纪要** | [`qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md`](qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md)（含 512 起草节） |

### 512 草案默认（待确认）

S-1 单 cube；T-B2 polyvec4；`-k2`；CBD‑η3 新积木；保留 D19–D21+CT；PKE exp 必做；本阶段 **liboqs-512 交叉必达**；禁零垫 / 禁 stable-512；锁卡后 **自主推进**。

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| **T512** | 用户勾选参数卡 §0 / 计划 §9 → 落 P0 目录壳与 sizes/helper → P1 定稿 → W0 |
| T768-post | 可选；非挡 512 |
| stable-768/512 | 仅 `#交付#` |

---

## ★ Smoke

```bash
bash scripts/check_mlkem768_sizes.sh
test -f docs/research/MLKEM-512-从0到exp完整实现计划.md
test -f docs/specs/fips203-mlkem512-parameter-card.md
test ! -d examples/stable/ml-kem/ml-kem-512
```

---

## ★ 勿做

- 未锁 §决议就写 512 kernel / 建 stable-512  
- 从 `**/frozen/**` 抄实现；零垫凑 3/4/6/8  
- 无 customspec 改 `examples/incubating/ml-kem/ml-kem-512/`  
- 擅自改已锁参数（锁后）
