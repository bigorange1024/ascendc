# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-26（768 从 0→exp **完整计划草案待审**；1024 迁移已冒烟绿）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`** |
| **目录结构** | 活跃仅 `…/ml-kem/ml-kem-1024/`；**`ml-kem-768/` 未建** |
| **768 计划** | 草案 [`docs/research/MLKEM-768-从0到exp完整实现计划.md`](docs/research/MLKEM-768-从0到exp完整实现计划.md) — **待用户拍板 §9** |
| **768 前提** | 做 **1–3**；**不做 4**（零垫）；终点 **incubating KEM 三算子**（非 stable） |
| **1024 迁移** | 冒烟 `TOTAL_FAIL=0`；幽灵 none |

---

## ★ 下一刀（P0）— 审阅并锁定 768 计划后开文书

1. 用户审阅计划 §9（分核 T-B、D19–D21、CT、是否跳过 PKE exp、命名 `-k3`）  
2. 通过后：P0 参数卡 + CT + 目录壳（仍不写 kernel）  
3. 再按 W0→W4 波次推进

---

## ★ Smoke（路径自检）

```bash
test -d ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-k4
test -d examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4
bash scripts/cleanup-ascendc-test-ghosts.sh   # 期望 none
# 端到端（需 CANN/liboqs）：
CPU_TRIALS=1 SIM_TRIALS=1 bash scripts/stable_kem_liboqs_roundtrip.sh
```

---

## ★ 勿做

- 从 `**/frozen/**` 抄实现进活跃树
- 未确认 customspec 就在 `examples/` 写码
- 未压测绿就建 `stable` / `ml-kem-768` stable
- 擅自改 `.cursor/rules/` / `.cursor/skills/`
