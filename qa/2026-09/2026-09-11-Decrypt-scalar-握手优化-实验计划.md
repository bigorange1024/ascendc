# 2026-09-11 · Decrypt scalar/握手优化 · 实验计划落盘

## 决策

- 用户要求：**先列齐实验计划**（含图谱推理），计划齐了直接上机测；Agent 须对计划质量负责。  
- Launch 压缩已收口；下一战役不是再压 launch，而是按 profiling 证据优化 D09 融合核 **scalar / 跨核握手**。

## 落盘

| 资产 | 路径 |
|------|------|
| 战役入口 | `graph-tests/decrypt-scalar-opt/INDEX.md` |
| 全套计划 | `graph-tests/decrypt-scalar-opt/PLAN.md` |
| 队列 | `graph-tests/decrypt-scalar-opt/QUEUE.md` |
| W1 热点清单 | `graph-tests/decrypt-scalar-opt/tasks/DS-W1/HOTSPOTS.md`（离线完成） |
| 推理图谱 | `docs/rg-decrypt-scalar-opt.yaml` |
| 交接 | `AGENT_HANDOFF.md` P0 已指向本战役 |

## 推荐上板序

**W0 基线复测 → H2 Prep/解码向量化 → H3 NTT/INTT 向量化 → H1 握手减薄 → H4 双 AIV（按需）**

## 下一动作

板上执行 `DS-W0`（`RB-D09-decrypt-1launch` 复测 µs/scalar%），通过后开 `RB-D10b-prep-vec`。
