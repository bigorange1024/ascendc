# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-15（Encaps customspec 已写；待确认后【预研】；下一亦可 T19b/c）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **T19a** Encaps device | [`pass-fix-f203-alg20-kem-encaps-device-k4`](ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/) **已更名 pass-fix**；CPU+SIM PASS；tick **721010** |
| **Encaps incubating 规格** | [`exp-fips203-mlkem-kem-encaps-k4`](examples/incubating/exp-fips203-mlkem-kem-encaps-k4/) **仅 customspec**（tex+pdf）；**无实现码** |
| PKE + KEM KeyGen stable | 已交付；tick 见 [`qa/active_sim_regress_summary.md`](qa/active_sim_regress_summary.md) |
| `docs/research/` | **已恢复**（调研草稿区；定稿仍进 `notes/`） |

---

## ★ 下一刀（P0）

1. 确认 Encaps customspec 后 **【预研】写码**（指明 `exp-…-kem-encaps-k4-实现方案-customspec`）；或
2. **T19b/c** — [`fix-f203-alg21-kem-decaps-device-k4`](ascendc-tests/fix-f203-alg21-kem-decaps-device-k4/)：Phase-E/D；禁止 frozen G5/G4 抄码。

---

## ★ Smoke

```bash
# device 基线
cd ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
# 规格 PDF（仅查阅，无 run.sh）
ls examples/incubating/exp-fips203-mlkem-kem-encaps-k4/*-customspec.pdf
```

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| `runtime_env` | 活跃探针 `run.sh` 已接入；默认仍 **cpu** |
| thirdparty | `ntt_onnx` 靠 **`ASCENDC_GH_PAT`**；`ensure_thirdparty_dep` 可按需补 `tiny_sha3` |
| Cloud SIM dump | 非 WSL 不装 dump 桩（`sim_env.sh`） |
