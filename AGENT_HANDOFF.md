# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-17（**T2 PASS**：Decaps SIM 单库 + 默认 1-session）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **KEM Encaps stable** | [`stable-fips203-mlkem-kem-encaps-k4`](examples/stable/stable-fips203-mlkem-kem-encaps-k4/) **定型**；tick **721119** |
| T19a Encaps device | [`pass-fix-…-encaps-device-k4`](ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/)；tick **721010** |
| **T19b/c + T2 Decaps device** | [`fix-…-decaps-device-k4`](ascendc-tests/fix-f203-alg21-kem-decaps-device-k4/) **全链 + E3 + 单库/1-session PASS**；D**286803**+E**745925**；`scripts/` Decaps 默认已指 device |
| PKE + KEM KeyGen stable | 已交付 |

---

## ★ 下一刀（P0）

| 优先级 | 事项 |
|--------|------|
| **P0** | Decaps：`pass-fix-…decaps-device-k4` **更名**；KAT 扩量；更后 `#交付#` → stable |
| **P1** | T21 SHA3hp 拍板；NPU 实机（PKE/KEM） |

**本轮不做**：再改 Decaps Alg.18 / 回退双库。

---

## ★ Smoke

```bash
cd ascendc-tests/fix-f203-alg21-kem-decaps-device-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4   # 默认单库 + decaps_1session
# 对照（非默认）：ASCENDC_SIM_HOST_MODE=decaps_2session bash run.sh -r sim …
```

---

## ★ 约束速记

- frozen：进门读判决书，出门不带码  
- examples 写码须活跃 `*-customspec.*`；ascendc-tests 不受此限  
- 已锁定参数不得擅自改；写 AscendC 前查 API 查阅索引  
- Decaps SIM：**单库 + 默认 1-session**（T2）；`prepare_dec_shim.sh` 生成 `shim/`（gitignore）  
