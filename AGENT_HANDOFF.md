# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-20（Decaps `#交付#` stable；六算子齐；纪要/备份/合入 main）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **KEM Encaps stable** | [`stable-…-kem-encaps-k4`](examples/stable/stable-fips203-mlkem-kem-encaps-k4/) **定型**；tick **721119** |
| **KEM Decaps stable** | [`stable-…-kem-decaps-k4`](examples/stable/stable-fips203-mlkem-kem-decaps-k4/) **定型**（2026-07-20 `#交付#`）；`DECAPS_DIR` 默认已指 |
| **Alg.21 Decaps device** | [`pass-probe-…-decaps-device-k4`](ascendc-tests/pass-probe-f203-alg21-kem-decaps-device-k4/) **PASS**（D**286803**+E**745925**） |
| PKE + KEM KeyGen/Encaps/Decaps | **六算子 stable 齐** |

---

## ★ 下一刀（P0）

| 优先级 | 事项 |
|--------|------|
| **P1** | Decaps SIM `fo_only`→`l18_l19` 尾（4→3 launch） |
| **P1** | NPU 实机压测（T2-npu） |

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
