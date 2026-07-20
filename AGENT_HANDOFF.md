# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-20（stable KEM ↔ liboqs **随机字节** roundtrip **CPU+SIM 全绿**；PR `cursor/liboqs-kem-vs-m-file-8244`）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **KEM 三件套 stable** | KeyGen / Encaps / Decaps **均已定型**；六算子齐 |
| **办公室回归入口** | [`scripts/stable_kem_liboqs_roundtrip.sh`](scripts/stable_kem_liboqs_roundtrip.sh)：**先** liboqs `urandom`（64B `kem_seed`+32B `m`）→ **再**同字节喂 AscendC；默认 **CPU×1 + SIM×1** |
| **证据（2026-07-20）** | CPU+SIM 全绿（KeyGen/Encaps/Decaps accept+reject max=0）；fixture `output/stable_kem_liboqs_rt/20260720_092533_195335/`；Decaps SIM D**286851**+E**746275** |
| **Alg.21 Decaps device** | [`pass-fix-…-decaps-device-k4`](ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/) **PASS**（D**286803**+E**745925**）；**禁**旧名 `fix-…-device` / 误名 `pass-probe-*` |
| **T13b / T11** | **已关闭（已取代）** |
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
# stable KEM ↔ liboqs（必跑；CPU+SIM 都绿才算数）
bash scripts/stable_kem_liboqs_roundtrip.sh

# 单算子 / 分项（可选）
cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
KEM_DEC_CPU_TRIALS=10 KEM_DEC_SIM_TRIALS=3 bash scripts/liboqs_kem_decaps_batch.sh
```

定点复现：`KEM_SEED_HEX=… M_HEX=… bash scripts/stable_kem_liboqs_roundtrip.sh`  
仅 CPU / 仅 SIM：`SKIP_SIM=1` / `SKIP_CPU=1`（**勿**当作办公室验收完成）。

---

## ★ 勿做

- 从 `**/frozen/**` 抄码；未 `#…#` 改 stable 实现
- WSL 上 `run.sh -r npu`
- Decaps 全链只传 `DK_KEM_SRC` 不传 `EK_KEM_SRC`（会回落 stash ek → FO 假拒）
- 把定点 `SEED_D` 的 `liboqs_kem_vs_ascendc.sh` 当成「随机字节」办公室回归（那是另一条路径）
