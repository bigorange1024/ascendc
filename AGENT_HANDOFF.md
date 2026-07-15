# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-15（T19a Encaps device PASS；恢复 `docs/research/`）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **T19a** Encaps device | [`fix-f203-alg20-kem-encaps-device-k4`](ascendc-tests/fix-f203-alg20-kem-encaps-device-k4/) **CPU+SIM PASS**；`c`/`K` max=0；SIM tick **721010** |
| PKE + KEM KeyGen stable | 已交付；tick 见 [`qa/active_sim_regress_summary.md`](qa/active_sim_regress_summary.md) |
| `docs/research/` | **已恢复**（调研草稿区；定稿仍进 `notes/`） |

---

## ★ 下一刀（P0）

**T19b/c** — [`fix-f203-alg21-kem-decaps-device-k4`](ascendc-tests/fix-f203-alg21-kem-decaps-device-k4/)：Phase-E 接 stable Encrypt 布局、Phase-D 接 stable Decrypt；禁止 frozen G5/G4 抄码。

可选收尾：Encaps 更名 `pass-fix-…`；仓库 `ENCAPS_DIR` / 分项 kat 改指 device。

---

## ★ Smoke

```bash
cd ascendc-tests/fix-f203-alg20-kem-encaps-device-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| `runtime_env` | 活跃探针 `run.sh` 已接入；默认仍 **cpu** |
| thirdparty | `ntt_onnx` 靠 **`ASCENDC_GH_PAT`**；`ensure_thirdparty_dep` 可按需补 `tiny_sha3` |
| Cloud SIM dump | 非 WSL 不装 dump 桩（`sim_env.sh`） |
