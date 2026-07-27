# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-27（**用语统一：缺项/补洞**；512 决议已锁；下一刀 P0）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（与 `main` @ `5b39d64` 对齐后再续；PR [#13](https://github.com/bigorange1024/ascendc/pull/13) 已合入） |
| **768** | incubating + glue **有条件完成**；**真 liboqs 交叉 RT 已绿**；禁 stable-768 |
| **512** | **P0 决议已锁**；KEM liboqs 胶水可切 512/768/1024；**目录壳/写码未开** |
| **512 计划** | [`docs/research/MLKEM-512-从0到exp完整实现计划.md`](docs/research/MLKEM-512-从0到exp完整实现计划.md)（含用语约定） |
| **512 参数卡** | [`docs/specs/fips203-mlkem512-parameter-card.md`](docs/specs/fips203-mlkem512-parameter-card.md) |
| **用语** | 只用教材已定义：**缺项** / **补洞** / **补洞图**=缺项对照表；**禁止**「洞 / 新洞 / 洞级」 |
| **当日纪要** | [`qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md`](qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md) |

### 512 已锁默认

S-1 单 cube；T-B2 polyvec4；`-k2`；CBD‑η3 为相对 768 的**缺项**；D19–D21+CT；PKE exp 必做；本阶段 **liboqs-512 交叉必达**；禁零垫 / 禁 stable-512；**自主推进**（仅停问点打断）。

---

## ★ 下一刀

| 项 | 说明 |
|----|------|
| **T512** | 落 P0 目录壳 / `check_mlkem512_sizes.sh` → P1 缺项对照定稿 → W0 |
| T768-post | 可选；非挡 512 |
| stable-768/512 | 仅 `#交付#` |

---

## ★ Smoke

```bash
bash scripts/smoke_liboqs_kem_params.sh
bash scripts/check_mlkem768_sizes.sh
test -f docs/specs/fips203-mlkem512-parameter-card.md
test ! -d examples/stable/ml-kem/ml-kem-512
```

---

## ★ 勿做

- 建 stable-512；从 `**/frozen/**` 抄实现；零垫凑 3/4/6/8  
- 无 customspec 改 `examples/incubating/ml-kem/ml-kem-512/`  
- 擅自改已锁参数  
- **擅自新建方法论用语**（须用教材已定义名词）
