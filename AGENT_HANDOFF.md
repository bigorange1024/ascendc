# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-26（工程结构：活跃 ML-KEM 迁入 `ml-kem/ml-kem-1024/`；下一刀 768 方法论）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`**（结构迁移在此；交付测树仍可对齐 `main`） |
| **目录结构** | 活跃 ML-KEM 已迁入三树 `…/ml-kem/ml-kem-1024/`；**frozen 未搬**；详 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| **KEM 六算子 stable** | 路径前缀现为 `examples/stable/ml-kem/ml-kem-1024/stable-…`；`scripts/` 默认已更新 |
| **Decaps 交付 / CT** | 仍分别为无 `-ct` / `-ct`；语义不变 |
| **768 实验前提** | 用户确认做 **1–3**（真 768、重做形状/tiling、exp 层 KEM 三算子）；**不做 4**（零垫探针） |

---

## ★ 下一刀（P0）— ML-KEM-768 方法论（结构已就绪）

**目标**：按已确认前提开 768 从 0 到 1（先文书/参数卡+CT，再到 exp KEM 三算子）。  
**落点**：各活跃树新建 `ml-kem/ml-kem-768/`（**尚未创建**；开写前再确认）。  
**约束**：k4/1024 只作已获证模式参考；禁止用 k4 末 poly 置零冒充 768；stable 非必达。

建议序（与此前讨论一致）：

1. **P0** 参数卡 + CT（形状/tiling/I/O 锁定）
2. **P1** 补洞路线图（相对 1024 缺什么）
3. **P2** 按洞：分析 → 文书 → 代码 → 测试
4. **P3** 闭环判决（做到 exp 层 KEM 三算子即可）

---

## ★ Smoke（路径自检）

```bash
test -d ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-k4
test -d examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4
# 端到端（需 CANN/liboqs）：
bash scripts/stable_kem_liboqs_roundtrip.sh
```

---

## ★ 勿做

- 从 `**/frozen/**` 抄实现进活跃树
- 未确认 customspec 就在 `examples/` 写码
- 未压测绿就建 `stable` / `ml-kem-768` stable
- 擅自改 `.cursor/rules/` / `.cursor/skills/`
