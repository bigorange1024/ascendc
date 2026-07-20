# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-20（清理 Decaps 更名幽灵 + `pass-probe` 误链；挂账 T23；关闭 T13b/T11）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **KEM Encaps stable** | [`stable-…-kem-encaps-k4`](examples/stable/stable-fips203-mlkem-kem-encaps-k4/) **定型**；tick **721119** |
| **KEM Decaps stable** | [`stable-…-kem-decaps-k4`](examples/stable/stable-fips203-mlkem-kem-decaps-k4/) **定型**（2026-07-20 `#交付#`）；`DECAPS_DIR` 默认已指 |
| **Alg.21 Decaps device** | [`pass-fix-…-decaps-device-k4`](ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/) **PASS**（D**286803**+E**745925**）；**禁**旧名 `fix-…-device` / 误名 `pass-probe-*` |
| PKE + KEM KeyGen/Encaps/Decaps | **六算子 stable 齐** |
| **T13b / T11** | **已关闭（已取代）**：设备 Â+V3+2s1e 已在 KeyGen stable |
| **T23** | **打开**：2+ AI Core，每 Core 一路独立 stable（乃至 round-trip） |

---

## ★ 下一刀（P0）

| 优先级 | 事项 |
|--------|------|
| **P1** | **T23** 多 AI Core 并行 stable（先 2 Core；每 Core 一份算子 / round-trip） |
| **P1** | **T19i** Decaps SIM `fo_only`→`l18_l19` 尾（4→3 launch） |
| **P1** | **T2-npu** NPU 实机压测 |
| **P1** | **T21** SHA3hp 拍板 |

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
