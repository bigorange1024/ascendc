# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-18（Decaps incubating **`$规格$` customspec**；device 已 `pass-fix`）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **KEM Encaps stable** | [`stable-fips203-mlkem-kem-encaps-k4`](examples/stable/stable-fips203-mlkem-kem-encaps-k4/) **定型**；tick **721119** |
| T19a Encaps device | [`pass-fix-…-encaps-device-k4`](ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/)；tick **721010** |
| **Alg.21 Decaps device** | [`pass-fix-…-decaps-device-k4`](ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/) **PASS**（单库+1-session；D**286803**+E**745925**；KAT 10+3；roundtrip CPU/SIM）；`scripts/` 默认已指 |
| **Alg.21 Decaps incubating** | [`exp-fips203-mlkem-kem-decaps-k4`](examples/incubating/exp-fips203-mlkem-kem-decaps-k4/)：**仅** [customspec](examples/incubating/exp-fips203-mlkem-kem-decaps-k4/exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.pdf)（2026-07-18）；无实现码 |
| PKE + KEM KeyGen stable | 已交付 |

---

## ★ 下一刀（P0）

| 优先级 | 事项 |
|--------|------|
| **P0** | 用户确认 Decaps customspec → **`【预研】`** 写 `exp-fips203-mlkem-kem-decaps-k4`（指明该路径） |
| **P1** | incubating 绿后 **`#交付#`** → `examples/stable`；可选 SIM `fo_only`→`l18_l19` 尾（4→3） |

---

## ★ Smoke

```bash
cd ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

---

## ★ 约束速记

- frozen：进门读判决书，出门不带码  
- examples 写码须活跃 `*-customspec.*`；ascendc-tests 不受此限  
- 已锁定参数不得擅自改；写 AscendC 前查 API 查阅索引  
- **只在 `origin/main` 工作**；未经用户指令不提交、不推送  
