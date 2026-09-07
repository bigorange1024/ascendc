# CP1 用户结论（白话锁定 · 2026-09-07）

| 你说的 | 主控理解（已锁定） |
|--------|-------------------|
| 「1 不明白」 | **不用你懂那些芯片缩写**。继续用本仓一直在用的写法：910B 上 **Cube+Vector 混核直调**。 |
| 「2 继续通过图谱做实验，引入 cannbot skills」 | 实验仍走 **`graph-tests/` + KB/DAG`**；每刀编码/改核 **主动用** `thirdparty/cannbot-skills`（尤其 sync-audit / CrossCore / crash-debug），不另开 ACLNN 大工程。 |
| 「3 正确性不是现在核心」 | 核心仍是 **NPU 不卡死**；liboqs/逐字节 **延后**。 |
| 「4 不明白、与核心无关」 | **Host μ 不再当讨论项**；方案里由主控自定，不挡实验。 |
| 「5 没必要 vendor」 | **不改** `.cursor/skills`；需要时直接读/跑 `thirdparty/cannbot-skills`。 |

---

## 已用 cannbot 的第一刀（只读）

对冻结旧核 `…/compute/f203_encrypt_l18_l19_kernel.cpp` 跑了 `ascendc-sync-audit`：

- 原始输出：`.cannbot/mlkem-pke-encrypt/tmp/sync_audit_l18_l19.json`
- 摘要：大量 **SYNC-09**（`PipeBarrier<PIPE_ALL>` 过粗/过密，偏性能）；一条 **SYNC-03**（CrossCore 同侧配对可疑，`flag=st`，可能宏/别名误报，**不能单独当卡死根因**）
- 用途：新 `enc_related` 实验的负面对照与每刀门禁样例——**禁止据此去改冻结旧核**

---

## 下一刀（图谱）

停「同质 toys」；开 **`graph-tests/enc_related/`** 近 Encrypt 体量实验，强制：

1. 一刀一目录 + 任务书挂 DAG  
2. 编码后跑 cannbot `sync_audit.py`  
3. 遵守 KB 禁令（5/7、Wait 中 SyncAll、自造 SoftSync、抄旧 Encrypt）  
4. 验收：SIM 不挂（CPU+SIM）；正确性非门禁  
5. **不连 910B3** 除非你当次点名
