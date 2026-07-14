# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-14（Cloud 二次失败清单本仓已修；请 Cloud **只测不改**）

---

## ★ Cloud 下一刀（优先）

1. `git pull` 到本交接对应 HEAD；**不要改代码**  
2. 先：`FORCE=1 ONLY=tiny_sha3,ntt_onnx BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh`（或依赖 run.sh 按需 ensure）  
3. 重跑先前仍红子集：CBD / byteencode12 / se-k4 / device-keygen / shake128·256 /（可选）vec-k4-v2 CPU  
4. vec-k4-v2 若 CPU 仍 `ek_pke` 红：贴完整 verify；本机已绿  

说明：[`qa/2026-07/2026-07-14-KEM-KeyGen-incubating预研重写.md`](qa/2026-07/2026-07-14-KEM-KeyGen-incubating预研重写.md)

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| `runtime_env` | 活跃探针 `run.sh` 已接入；默认仍 **cpu** |
| thirdparty | `clone-thirdparty.sh`；**`ntt_onnx` 私有**靠 **`ASCENDC_GH_PAT`**；`ensure_thirdparty_dep.sh` 按需补 `tiny_sha3` |
| Cloud SIM dump | 非 WSL 不装 dump 桩（`sim_env.sh`） |
