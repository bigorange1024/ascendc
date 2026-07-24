# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-24（教材第6章复盘成文；工程主线见下）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **形式方法教材** | 分支 [`research/formal-lang-dag`](docs/research/)；**第6章复盘已成文**（KeyGen/Encaps 事后 CT + 结论）；第4–5章算法细节暂不细抠 |
| **KEM Encaps / KeyGen / PKE** | 均已 `examples/stable/` 定型 |
| **Decaps** | 有 correctness 标本 + `pass-fix-…-decaps-device-k4`；**第7章方法论路径尚未按「先 CT 再写码」闭环** |

---

## ★ 下一刀（P0）

1. **第7章前置**：`kem.decaps` 先交真闭包表 CT（多父 + FO 接缝 + Forbidden）  
2. 按 CT → customspec → 实现 → CPU+SIM；再写第7章并判决方法论  
3. 全程继续在 **`research/formal-lang-dag`**（不合入 `main`，除非用户明确指令）

---

## ★ Smoke

```bash
cd examples/stable/stable-fips203-mlkem-kem-encaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| `runtime_env` | 活跃探针 `run.sh` 已接入；默认仍 **cpu** |
| thirdparty | `ntt_onnx` 靠 **`ASCENDC_GH_PAT`**；KEM golden 用 `liboqs_kem_ref` |
| Cloud SIM dump | 非 WSL 不装 dump 桩（`sim_env.sh`） |
| git | 专题文档可在本分支持续 commit/push |
