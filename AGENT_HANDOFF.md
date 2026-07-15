# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-15（【预研】Encaps incubating CPU+SIM PASS）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **T19a** Encaps device | [`pass-fix-f203-alg20-kem-encaps-device-k4`](ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/) pass-fix；tick **721010** |
| **Encaps incubating** | [`exp-fips203-mlkem-kem-encaps-k4`](examples/incubating/exp-fips203-mlkem-kem-encaps-k4/) **CPU+SIM PASS**；tick **≈721k**；vendored Encrypt + kem 头 |
| PKE + KEM KeyGen stable | 已交付 |
| `docs/research/` | 已恢复 |

---

## ★ 下一刀（P0）

1. Encaps `#交付#` → stable（可选，须压测绿后复制晋级）；或  
2. **T19b/c** Decaps device。

---

## ★ Smoke

```bash
cd examples/incubating/exp-fips203-mlkem-kem-encaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| `runtime_env` | 活跃探针 `run.sh` 已接入；默认仍 **cpu** |
| thirdparty | `ntt_onnx` 靠 **`ASCENDC_GH_PAT`**；Encaps golden 用 `liboqs_kem_ref` |
| Cloud SIM dump | 非 WSL 不装 dump 桩（`sim_env.sh`） |
