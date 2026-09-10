# Agent 交接（Launch 压缩 · NPU）

> **最后刷新**：2026-09-10（LR-DC-F2 Decrypt→1 **PASS_NPU×30**；已停 keepalive）  
> **分支**：`cursor/launch-reduce-npu-pass-23e1`  
> **入口**：[`graph-tests/launch-reduce-ops/PLAN.md`](graph-tests/launch-reduce-ops/PLAN.md)

---

## ★ 当前真相

1. Launch 压缩战役 **QUEUE 1–7 全绿**：KG=2、Decrypt=**1**、Decaps=3（对齐 stable 档位数）。  
2. **LR-DC-F2 关闸**：`RB-D09-decrypt-1launch` 独立新树；NPU 冒烟 + ×30（ok=30，换 SEED）；**未改** D08/T28/T29。  
3. 证据：`/mnt/workspace/launch-reduce-logs/d09-smoke-20260910-143857.log` · `d09-x30-20260910-144039.log`  
4. **禁止**本战役用 CPU/SIM 结案；强制 cannbot `sync_audit`。  
5. keepalive **已停**（放机）。

---

## ★ 开机后立刻做

1. `bash scripts/cannlab/which_npu.sh`（**勿**残留旧 `SSH_HOST_FORCE` IP）→ 需要上板再 keepalive  
2. 可选 Wave4：经验入库（KB / qa / `.cannbot`）——须用户授权再改 KB 图  
3. 无新刀则保持放机

---

## 硬约束摘要

禁抄 stable KeyGen/Decaps 核；flag∉{5,7}；禁 SoftSync；Wait 环禁 SyncAll；DC-F2 只写新树。
