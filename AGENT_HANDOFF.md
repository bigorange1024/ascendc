# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-26（**ML-KEM-768 P0+P1 已落地**；下一刀 P2/W0）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`** |
| **768 参数卡** | [`docs/specs/fips203-mlkem768-parameter-card.md`](docs/specs/fips203-mlkem768-parameter-card.md) **已锁**（T-B / `-k3` / CT / PKE+KEM exp） |
| **768 P1 用例表** | [`docs/specs/fips203-mlkem768-p1-gap-and-cases.md`](docs/specs/fips203-mlkem768-p1-gap-and-cases.md) **已定稿** |
| **目录壳** | `ascendc-tests/ml-kem/ml-kem-768/`（13 探针）· `examples/incubating/ml-kem/ml-kem-768/`（7 exp）— **无源码** |
| **stable-768** | **不建**（本阶段） |
| **1024** | 迁移已冒烟绿；继续只读作模式参考 |

### 用户决议（已写入参数卡）

1. 分核 **T-B**（polyvec6）+ Â 独立 prep  
2. 保留 KEM device D19–D21  
3. **要求** reject/CT（D21ct + E21ct）  
4. **PKE exp 也要做**  
5. 命名 `-k3`  
6. 本轮只做完 P0+P1  

---

## ★ 下一刀（P0）— 开 P2 / W0（须用户授权 + Skill）

**W0 积木**：B1 compress-decompress du10/dv4 · B2 byteencode-decode · B3 CBD η2×k3  

开写前：

- 探针：【预研】或等价授权  
- incubating：须 `$规格$` / 活跃 customspec（P2 波次到 W4 才写 exp）  
- 补齐参数卡 §3 **数值 tiling**（按波次，遇阻停问）  
- `bash scripts/check_mlkem768_sizes.sh`  

---

## ★ Smoke（路径自检）

```bash
bash scripts/check_mlkem768_sizes.sh
test -d ascendc-tests/ml-kem/ml-kem-768
test -d examples/incubating/ml-kem/ml-kem-768
test ! -d examples/stable/ml-kem/ml-kem-768
bash scripts/cleanup-ascendc-test-ghosts.sh
```

---

## ★ 勿做

- 从 `**/frozen/**` 抄实现；零垫凑 4/8  
- 无 customspec 写 `examples/incubating/ml-kem/ml-kem-768/**` 代码  
- 未压测绿建 `stable` / `ml-kem-768` stable  
- 擅自改 `.cursor/rules/` / `.cursor/skills/`  
