# ⛔ 已冻结（2026-06-15）

**原路径**：`ascendc-tests/fix-f203-tag5t-ntt256-limb6-poly8-polybatch-s123/`  
**继任活跃探针**：[`fix-f203-2s1e-alg13-16171820-k4`](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) — NTT + 行 18–20 UB 融合、平面 mat_c、无 Gather。

## 冻结原因

| 类别 | 说明 |
|------|------|
| **路线被取代** | 2026-06-12 曾作「NTT-only 权威探针」；现 MLKEM 集成以 **2s1e** 为唯一维护基线（含 S1–S3 + 行 18–20）。 |
| **Gather 路线（NTT S3）** | Stage3 `deinterleave_even_odd_vec` 依赖 `AscendC::Gather`；**三段式 NTT 内禁止**；已由 2s1e 平面 mat_c 取代。 |
| **mat_c 布局** | 竖堆 `mat_c[32,256]`（C_lo/C_hi 分行）；后续平面 `[96,128]` + bulk `DataCopy` 在 2s1e/planar 已验证更高效、可维护。 |
| **算力利用** | 仅 NTT 无下游 Alg.13；与 KeyGen 集成需重复 fork。2s1e 一次 MIX 覆盖 host `1s+1e` 与 ek/sk 输出。 |

## 仍可读对照

- poly-batch S0 行序、`AivSplitPolyBatch` / `AivTag5tRouteAModPolyBatch` 命名  
- 与 `frozen-fix-f203-tag5t-…-limbsplit-s123` 的 dst 对拍历史  

**勿 fork、勿跑 CI。**
