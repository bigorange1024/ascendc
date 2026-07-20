# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-20（T19i SIM 3 合入 · 冻结 correctness×3 · stable↔liboqs roundtrip · **main**）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **KEM 三件套 stable** | KeyGen / Encaps / Decaps **均已定型**；六算子齐 |
| **Alg.21 Decaps** | [`stable-…-kem-decaps-k4`](examples/stable/stable-fips203-mlkem-kem-decaps-k4/) **T19i SIM 3**（D**286851**+E**763769**）；基线 pass-fix 同构 |
| **Alg.19/20/21 correctness** | **已冻结** → `ascendc-tests/frozen/frozen-fix-…-*-correctness-k4/`（只读 FROZEN.md；**禁翻源码**） |
| **办公室回归入口** | [`scripts/stable_kem_liboqs_roundtrip.sh`](scripts/stable_kem_liboqs_roundtrip.sh)：**先** liboqs `urandom` → **再**同字节喂 AscendC；**CPU×1 + SIM×1** |
| **证据（2026-07-20）** | roundtrip CPU+SIM 全绿；fixture `output/stable_kem_liboqs_rt/20260720_092533_195335/` |
| **T6 / T7a / T7c / T19i** | **关闭** |
| **T23** | **打开**：多 AI Core∥stable |

---

## ★ 下一刀（P0）

| 优先级 | 事项 |
|--------|------|
| **P1** | **T23** 多 AI Core 并行 stable（先 2 Core） |
| **P1** | **T2-npu** / **T21** |

---

## ★ Smoke

```bash
# stable KEM ↔ liboqs（urandom→liboqs→同字节喂 AscendC；CPU+SIM 都绿才算数）
bash scripts/stable_kem_liboqs_roundtrip.sh

cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

定点复现：`KEM_SEED_HEX=… M_HEX=… bash scripts/stable_kem_liboqs_roundtrip.sh`

---

## ★ 勿做

- 从 `**/frozen/**` 抄码；未 `#…#` 改 stable 实现
- WSL 上 `run.sh -r npu`
- Decaps 全链只传 `DK_KEM_SRC` 不传 `EK_KEM_SRC`（stash ek → FO 假拒）
- **另起分支**做已确认交付/冻结；直接在 **main** 提交
