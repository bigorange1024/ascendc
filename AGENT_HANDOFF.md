# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-24（第7章 CT → Decaps device CPU+SIM PASS）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **形式方法教材** | 分支 [`research/formal-lang-dag`](docs/research/)；第6章复盘已成文；**第7章 CT 已先交并驱动实现** |
| **KEM Encaps / KeyGen / PKE** | 均已 `examples/stable/` 定型 |
| **Decaps device** | [`pass-fix-f203-alg21-kem-decaps-device-k4`](ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)：**CPU+SIM 合法 `K` max=0 PASS**；SIM 默认 `decaps_2session`；拒绝 CPU PASS |

---

## ★ 下一刀（P0）

1. 教材第7章 **实现后判决**占位改写（弱/强成功 + 与 correctness 三轴对照）
2. 可选：拒绝路径 SIM；`liboqs_kem_decaps_batch` 长测；`#交付#` Decaps（须用户授权，本轮未开 examples）

---

## ★ Smoke

```bash
cd ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| `runtime_env` | 活跃探针 `run.sh` 已接入；默认仍 **cpu** |
| thirdparty | `ntt_onnx` 靠 **`ASCENDC_GH_PAT`**；KEM golden 用 `liboqs_kem_ref` |
| Cloud SIM dump | 非 WSL 不装 dump 桩（`sim_env.sh`） |
| git | 专题文档可在本分支持续 commit/push |
