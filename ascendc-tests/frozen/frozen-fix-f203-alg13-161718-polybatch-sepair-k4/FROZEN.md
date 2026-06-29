# ⛔ 已冻结（2026-06-15）

**原路径**：`ascendc-tests/fix-f203-alg13-161718-polybatch-sepair-k4/`  
**继任**：[`fix-f203-2s1e-alg13-16171820-k4`](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)

## 冻结原因

| 类别 | 说明 |
|------|------|
| **se_pair + peer GM** | 行 18 需 `SHAT_PEER` workspace 与对端 ŝ **GM 交换**；算力浪费在同步/搬运，且 tikicpu **必须 5→4 两段** launch。 |
| **实现路线被批评** | 曾拆成多趟 GM RouteA + Hat + Encode；后改 `AivAlg13UbPipeline` 仍保留 se_pair 交错布局与 peer 依赖，复杂度高。 |
| **Gather 残留（行 18 草案）** | `hat_vec.hpp` 中 `coef_pairs_vec` 含 Gather；属已关闭 sepair 路线，**非**全局禁止 post-NTT Gather。 |
| **host 契约重** | `src` 为 se_pair 交错 `[s0,e0,s1,e1|…]`；2s1e 改为 **host 1×s+1e、设备内复制 ŝ**，更贴近交付。 |

## 历史价值

- 首个 poly-batch + 平面 S2 + 行 18–20 UB 融合试验田  
- `mod_variants.hpp` 三模宏、`AivAlg13UbPipeline` 结构 → 2s1e 简化版 `Aiv2s1eUbPipeline`  

**勿 fork、勿跑 CI。**
