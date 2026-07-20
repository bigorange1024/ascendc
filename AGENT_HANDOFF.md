# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-20（冻结 KEM **correctness×3**；清理幽灵；挂账 T23；T19i 仍开）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **KEM Encaps/Decaps/KeyGen stable** | 六算子 stable **齐** |
| **Alg.19/20/21 correctness** | **已冻结** → `ascendc-tests/frozen/frozen-fix-…-*-correctness-k4/`（只读 FROZEN.md；**禁翻源码**） |
| **Alg.21 Decaps device** | [`pass-fix-…-decaps-device-k4`](ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/) **PASS** |
| **T6 / T7a / T7c** | **关闭**（随 correctness 冻结） |
| **T23** | **打开**：多 AI Core∥stable |

---

## ★ 下一刀（P0）

| 优先级 | 事项 |
|--------|------|
| **P1** | **T23** 多 AI Core 并行 stable（先 2 Core） |
| **P1** | **T19i** Decaps SIM `fo_only`→`l18_l19`（若尚未合入） |
| **P1** | **T2-npu** / **T21** |

---

## ★ Smoke

```bash
cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
KEM_DEC_CPU_TRIALS=10 KEM_DEC_SIM_TRIALS=3 bash scripts/liboqs_kem_decaps_batch.sh
bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r cpu -v Ascend910B4
```

---

## ★ 勿做

- 从 `**/frozen/**` 抄码；未 `#…#` 改 stable 实现
- WSL 上 `run.sh -r npu`
