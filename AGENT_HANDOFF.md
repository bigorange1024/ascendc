# Agent 交接（Launch 压缩 · NPU）

> **最后刷新**：2026-09-10（LR-DC-F2 Decrypt→1 独立 D09 开刀）  
> **分支**：`cursor/launch-reduce-npu-pass-23e1`  
> **入口**：[`graph-tests/launch-reduce-ops/PLAN.md`](graph-tests/launch-reduce-ops/PLAN.md)

---

## ★ 当前真相

1. Launch 压缩战役 QUEUE 1–6 已绿（KG→2、Decrypt→2、Decaps→3）。  
2. **下一刀 / 进行中**：**LR-DC-F2** = 新建 `RB-D09-decrypt-1launch`（Decrypt→1）；**禁止改** D08/T28/T29 源码。  
3. T29 L1 曾作 Decrypt-1 旁证；本刀要求**独立用例**验收。  
4. **禁止**本战役用 CPU/SIM 结案；强制 cannbot `sync_audit`。  
5. **空闲**：NPU 空转不得超过 **3 分钟**（停心跳或连续开下一刀）。  
6. CANNLab 主机：**勿写死旧 IP**；用 `which_npu.sh` / `cannlab_pick_host`（当前常见 `cannlab-npu-1`）。

---

## ★ 开机后立刻做

1. `bash scripts/cannlab/which_npu.sh` 成功 → keepalive（**勿**残留 `SSH_HOST_FORCE` 旧 IP）  
2. 续战 DC-F2：sync_audit → 同步 D09 → NPU 冒烟 → ×30  
3. 无新刀则 **停 keepalive 放机**

---

## 硬约束摘要

禁抄 stable KeyGen/Decaps 核；flag∉{5,7}；禁 SoftSync；Wait 环禁 SyncAll；DC-F2 只写新树。
