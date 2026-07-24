# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-24（Decaps CT：压测收尾 · `-ct` 改名 · 中文注释 · 引用审计）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **专题分支** | **仅** [`research/formal-lang-dag`](.)（勿开 `cursor/*` 旁支；误建已删） |
| **形式方法教材** | [`docs/research/`](docs/research/) 第6–7章；第7章强成功 |
| **Decaps CT 树**（与 **main** 同名交付区分） | device / exp / stable 均带 **`-ct`** |
| **非 NPU 压测** | 拒绝 SIM + KAT 10+3 + roundtrip CPU/SIM **PASS**；NPU 未跑 |
| **五指标对照** | [`docs/research/2026-07-24-Decaps-correctness与CT五指标对照.md`](docs/research/2026-07-24-Decaps-correctness与CT五指标对照.md) |

### CT 三树路径（锁名）

| 角色 | 路径 |
|------|------|
| device | `ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-ct-k4` |
| incubating | `examples/incubating/exp-fips203-mlkem-kem-decaps-ct-k4` |
| stable | `examples/stable/stable-fips203-mlkem-kem-decaps-ct-k4` |

（**main** 上无 `-ct` 的同名目录为办公室交付主线，勿在本分支当默认路径。）

---

## ★ 下一刀（P0）

1. 教材 correctness 对照章 / 五指标可再加细表  
2. **NPU** 真机（有卡时）  
3. 其它主线按用户指定  

---

## ★ Smoke

```bash
cd ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-ct-k4
# 或 examples/stable/stable-fips203-mlkem-kem-decaps-ct-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| scripts `DECAPS_DIR` | 本分支默认 → `stable-…-decaps-ct-k4` |
| SIM | `ASCENDC_SIM_HOST_MODE=decaps_2session` |
| 拒绝 | Gate E3 = `KEM_DECAPS_REJECT=1` |
| CPU twin | 须 `M_FILE` 与 encaps `m` 一致 |
