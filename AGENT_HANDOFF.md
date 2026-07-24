# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-24（`#交付#` KEM Decaps → `examples/stable`）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **形式方法教材** | [`research/formal-lang-dag`](docs/research/)：第6–7章；第7章强成功 + **stable Decaps 交付落点** |
| **Decaps 交付** | [`stable-fips203-mlkem-kem-decaps-k4`](examples/stable/stable-fips203-mlkem-kem-decaps-k4/)：**CPU+SIM `K` max=0**（D**286866**+E**763780**）；incubating 副本保留 |
| **Decaps device** | [`pass-fix-…-decaps-device-k4`](ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)：**行为基线**（仍 PASS；非 CMake 依赖） |
| **PKE / KEM KeyGen / Encaps / Decaps** | 均已 `examples/stable/` |

---

## ★ 下一刀（P0）

1. （可选）Decaps 拒绝路径 SIM / `liboqs_kem_decaps_batch` 长测  
2. 教材 correctness 对照章可按第7章证据加细表  
3. 其它主线按用户指定

---

## ★ Smoke

```bash
cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| 专题分支 | 继续 `research/formal-lang-dag`（不合入 `main` 除非用户明确指令） |
| thirdparty | Decaps/Encaps 树内 vendored `ntt_onnx` LUT 头；liboqs 供 golden |
| SIM | 生产默认 `ASCENDC_SIM_HOST_MODE=decaps_2session` |
| scripts | `DECAPS_DIR` 默认 → stable Decaps |
