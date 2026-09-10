# Agent 交接（Launch 压缩 · NPU）

> **最后刷新**：2026-09-10（LR-DP-F3 Decaps→2 **PASS_NPU×30**；已停 keepalive）  
> **分支**：`cursor/launch-reduce-npu-pass-23e1`  
> **入口**：[`graph-tests/launch-reduce-ops/PLAN.md`](graph-tests/launch-reduce-ops/PLAN.md)

---

## ★ 当前真相

1. Launch 压缩 **QUEUE 1–8 全绿**：KG=2、Decrypt=1、Decaps=**2**（**优于** stable Decaps=3）。  
2. **LR-DP-F3 关闸**：`RB-T30-decaps-2launch` 独立新树；NPU 冒烟 + ×30；**未改** T28/T29/D08/D09。  
3. 证据：`t30-smoke-20260910-153112.log` · `t30-x30-20260910-153535.log`  
4. keepalive **已停**（放机）。

---

## ★ 开机后立刻做

1. `which_npu.sh`（勿残留旧 `SSH_HOST_FORCE`）→ 需上板再 keepalive  
2. 可选 Wave4 经验入库（改 KB 须授权）  
3. 无新刀则保持放机
