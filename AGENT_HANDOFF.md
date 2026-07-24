# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-24（Decaps 非 NPU 压测收尾：拒绝 SIM + KAT + roundtrip）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **形式方法教材** | [`research/formal-lang-dag`](docs/research/)：第6–7章；第7章强成功 + **stable Decaps 交付落点** |
| **Decaps 交付** | [`stable-fips203-mlkem-kem-decaps-k4`](examples/stable/stable-fips203-mlkem-kem-decaps-k4/)：合法 CPU+SIM；**拒绝 CPU+SIM**；**KAT CPU×10+SIM×3**；**roundtrip CPU+SIM（含 E3）** |
| **Decaps device / exp** | 行为基线与 incubating 副本：拒绝 SIM 亦 **PASS** |
| **PKE / KEM KeyGen / Encaps / Decaps** | 均已 `examples/stable/` |

---

## ★ 下一刀（P0）

1. 教材 correctness 对照章可按第7章证据加细表  
2. **NPU** 真机冒烟（仅有卡环境；本 Cloud 未跑）  
3. 其它主线按用户指定

---

## ★ Smoke

```bash
cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 批测（勿并行多路 SIM）
bash scripts/liboqs_kem_decaps_batch.sh
SIM_DIRECT=1 bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r sim -v Ascend910B4
```

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| 专题分支 | 继续 `research/formal-lang-dag`（Cloud PR 镜像：`cursor/kem-decaps-non-npu-tests-5334`） |
| thirdparty | Decaps/Encaps 树内 vendored `ntt_onnx` LUT 头；liboqs 供 golden |
| SIM | 生产默认 `ASCENDC_SIM_HOST_MODE=decaps_2session` |
| scripts | `DECAPS_DIR` 默认 → stable Decaps；CPU twin 须 `M_FILE`（与 encaps `m` 一致） |
| 拒绝 | Gate E3 = `KEM_DECAPS_REJECT=1`（随机假密文）；`TAMPER_C` 仅为别名 |
