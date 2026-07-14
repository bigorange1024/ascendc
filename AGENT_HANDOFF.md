# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-14（Cloud 第二波全绿；device-keygen tick~542k @ `995efdd`）

---

## ★ Cloud 下一刀（优先）

第二波失败子集在 `995efdd` **已全部 CPU+SIM 绿**（含 `pass-fix-f203-alg13-device-keygen-k4`）。无强制续跑。

可选：全活跃探针矩阵扫一遍；或回主线（T19 Encaps / KEM KeyGen incubating）。

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| `runtime_env` | 活跃探针 `run.sh` 已接入；默认仍 **cpu** |
| thirdparty | `ntt_onnx` 靠 **`ASCENDC_GH_PAT`**；`ensure_thirdparty_dep` 可按需补 `tiny_sha3` |
| Cloud SIM dump | 非 WSL 不装 dump 桩（`sim_env.sh`） |
