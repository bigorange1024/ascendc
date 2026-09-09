# KGR-P01 — PKE KeyGen L1 prep 壳

> **runner**: `subagent_cpu_sim`（本机 CPU；SIM 仅当本刀点名）  
> **npu_owner**: main（本刀不要求 NPU）  
> **实现目录**: `graph-tests/kg_related/RB-K01-kg-prep/`（新建）

## 测什么（人话）

Alg.13 准备段：由约定 `seed_d` 得到 \((ρ,σ)\)，再生成矩阵 Â 与噪声向量 ŝ、ê（尚未 NTT / 点积 / Encode）。

## 对拍谁

Host / FIPS 风格 oracle（可用 `library/shared` + 分项探针约定的布局）；**本刀可不接 liboqs 全链**，但中间张量须可解释、可 cmp。

## 拓扑约束（KB §B2）

- 单 launch **AIV-only**；basename：`kg_prep_custom`（全局唯一）。  
- **禁止**本刀引入 CrossCore / MIX。  
- 若 `blockDim=2` 分写 Â：区间不交叠；CBD/PRF 段屏障语义写清（参考 prep 双 AIV **失败史笔记**，禁抄旧 prep 源码）。  
- 推荐首版 **`blockDim=1`** 降低半写风险，绿后再论证并行。

## 禁抄

见 `keygen-rebuild-ops/COMMON.md`：禁任何 KeyGen 算子级树；禁大段照抄 alg7/8 探针进本核（可 `#include` shared、可读 STATUS 契约）。

## 交付物

1. `RB-K01-kg-prep/`：`run.sh`、host、kernel、`gen_data`/verify、中文注释（与实现同轮）。  
2. CPU：`bash run.sh -r cpu -v Ascend910B4` → SUCCESS。  
3. `logs/sync_audit.json`（即便无 CrossCore 也跑 audit）。  
4. `FEEDBACK.md`（最短格式）。

## 验收语言

```text
测的是：KeyGen prep（ρ/σ + Â + ŝ/ê）
对拍：host/FIPS oracle（写明路径）
结果：max=0 / FAIL 原因
```

## 下一刀

PASS → KGR-P02（NTT ŝ/ê）。
