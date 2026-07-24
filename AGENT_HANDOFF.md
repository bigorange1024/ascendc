# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-24（第7章强成功：Decaps device CPU+SIM 复验 PASS）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **形式方法教材** | [`research/formal-lang-dag`](docs/research/)：第6章复盘 + **第7章 CT→实现→强成功判决**已成文 |
| **Decaps device** | [`pass-fix-f203-alg21-kem-decaps-device-k4`](ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)：合法路径 **CPU+SIM `K` max=0**（本机复验）；拒绝 CPU PASS；无 vendor；挂 stable D/E |
| **PKE / KEM KeyGen / Encaps** | 均已 `examples/stable/` |

---

## ★ 下一刀（P0）

1. （可选）Decaps 拒绝路径 SIM / liboqs KAT 批测  
2. **须用户授权**：`#交付#` → `examples/` Decaps exp/stable（本轮故意未开）  
3. 教材 correctness 对照章可按第7章证据加细表

---

## ★ Smoke

```bash
cd ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| 专题分支 | 继续 `research/formal-lang-dag`（不合入 `main` 除非用户明确指令） |
| thirdparty | Encrypt stable 的 `thirdparty` 软链；liboqs 供 golden |
| SIM | 生产默认 `ASCENDC_SIM_HOST_MODE=decaps_2session` |
