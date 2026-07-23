# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-23（形式方法教材第3章导论冻结；工程主线仍见下）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **KEM Encaps stable** | [`stable-fips203-mlkem-kem-encaps-k4`](examples/stable/stable-fips203-mlkem-kem-encaps-k4/) **定型**；tick **721119**；liboqs KAT **10+3 PASS** |
| T19a Encaps device | [`pass-fix-…-encaps-device-k4`](ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/) 行为基线；tick **721010** |
| PKE + KEM KeyGen stable | 已交付 |
| `docs/research/` | 已恢复 |
| **形式方法教材第3章** | 分支 [`research/formal-lang-dag`](docs/research/)；导论故事线已定（引子→问题→方法→工具）；**整体先保持**，汇报时再从此底稿删减 |

---

## ★ 下一刀（P0）

1. **T19b/c** Decaps device  
2. （并行文档）形式方法教材：第3章暂冻；有汇报需求再砍篇幅

---

## ★ Smoke

```bash
cd examples/stable/stable-fips203-mlkem-kem-encaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4   # WSL/Cloud：勿再手写 SIM_DIRECT=1
```

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| `runtime_env` | 活跃探针 `run.sh` 已接入；默认仍 **cpu** |
| thirdparty | `ntt_onnx` 靠 **`ASCENDC_GH_PAT`**；Encaps golden 用 `liboqs_kem_ref` |
| Cloud SIM dump | 非 WSL 不装 dump 桩（`sim_env.sh`） |
| git | 听用户指令再 commit/push；本轮已按指令推送 |
