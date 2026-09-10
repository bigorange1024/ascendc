# Agent 交接（Launch 压缩 · NPU）

> **最后刷新**：2026-09-10（LR-DP-F2 Decaps→3 关闸）  
> **分支**：`cursor/kem-2launch-sticky-1534`（工作区有未提交上板产物）  
> **Git**：无用户指示 **不** commit/push  
> **入口**：[`graph-tests/launch-reduce-ops/PLAN.md`](graph-tests/launch-reduce-ops/PLAN.md)

---

## ★ 当前真相

1. Launch 压缩战役 **QUEUE 1–6 全绿**（KG→2、Decrypt→2、Decaps→3）。  
2. 最新关闸：**RB-T29-decaps-3launch** = prep 融进 Decrypt MIX + Encaps2 → **3 launch**；NPU×30 ok=30。  
3. DC-F2（Decrypt→1）未单独建 D09：证据并入 T29 L1（`dec_decrypt_custom`）。若需独立 Decrypt-1 用例再开刀。  
4. **禁止**本战役用 CPU/SIM 结案；强制 cannbot `sync_audit`。  
5. **空闲**：NPU 空转不得超过 **3 分钟**（停心跳或连续开下一刀）。

---

## ★ 开机后立刻做

1. `bash scripts/cannlab/which_npu.sh` 成功 → keepalive  
2. 若续战：Wave4 经验入库（KB / qa / `.cannbot`）——须用户授权再改 KB 图  
3. 无新刀则 **停 keepalive 放机**

---

## 硬约束摘要

禁抄 stable KeyGen/Decaps 核；flag∉{5,7}；禁 SoftSync；Wait 环禁 SyncAll；无指示不推送。
