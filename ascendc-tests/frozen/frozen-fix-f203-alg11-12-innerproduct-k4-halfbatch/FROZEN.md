# ⛔ 已冻结（2026-06-17）

**原角色**：`pass-fix-f203-alg11-12-innerproduct-k4` 内 **二期 half 批处理**路线（`ProcessHalfBatch`、`INNERPRODUCT_HALF_BATCH=1`）  
**继任**：[`pass-fix-f203-alg11-12-innerproduct-k4`](../../pass-fix-f203-alg11-12-innerproduct-k4/) **一期全 poly**（`ProcessFullPoly`，`p→j`，4× `alg11_ub::compute_on_ub`）

**本目录内容**：二期源码快照（只读留档），**勿 fork、勿抄码、勿跑 CI**。

---

## 冻结原因

| 类别 | 说明 |
|------|------|
| **SIM 无性能收益（2×2×1）** | 收敛后二期+aBlockQue **~23220 tick** vs 一期 **~23248 tick**（`ALG11_VEC_OPTS=1`，差距在噪声内）。相对二期初版 ~26600 有优化，但**未 beat 一期**，小形状下 half 外层 + 4×half-basemul 维护抵消 GM/ŝ 缓存节省。 |
| **复杂度 / UB 成本高** | 二期需 `j→half→p` 三层循环、`cache_s_half_blanes`、`gather_f_half_pairs`、`aBlockQue_`（512 int）、`kScratchIntsHalf=1216` vs 一期 `5×N=1280` 且逻辑更直观。收益不足以承担集成与踩坑成本。 |
| **工程踩坑密度高** | scratch 重叠、half ∘ 须 TQue 等——详见 [docs/notes/F203-innerproduct-k4-技术总结.md](../../../docs/notes/F203-innerproduct-k4-技术总结.md) §3、§6。 |
| **4×4×1 未验证即不延续** | 二期原动机是更大 `P_OUT`/`S_VEC` 下 GM 批搬运；**未在 4×4×1 上证明 tick 优势**即不在活跃树维护双路线。若未来扩形状，应 **重新 spike**，不得默认 fork 本目录。 |
| **活跃探针应单一真相** | 内积探针对外只交付 **一期**；二期作为调研判决关闭，避免 Agent/协作者误用 `INNERPRODUCT_HALF_BATCH=1` 或抄 `hat_innerproduct_batch.hpp`。 |

---

## 放弃决策（2026-06-17）

1. **从活跃 kernel 删除** `ProcessHalfBatch`、`aBlockQue_`、`INNERPRODUCT_HALF_BATCH` 编译开关。  
2. **不再参考** 二期实现作行 18 内积或 2s1e 集成的编码模板。  
3. **行 18 / 内积下一跳**：仅用 [`pass-fix-f203-alg11-12-innerproduct-k4`](../../pass-fix-f203-alg11-12-innerproduct-k4/) 一期 + [`pass-fix-f203-alg11-12-multiplyntts-k4`](../../pass-fix-f203-alg11-12-multiplyntts-k4/) 的 `compute_on_ub`。  
4. **扩 4×4×1 或 +ê**：在活跃探针上 **新开实验**；若再试 half，先读本 `FROZEN.md` 与 qa 纪要，勿直接恢复本目录代码。

---

## 历史价值（只读）

- 首次在 **polyvec 内积**内验证 half-basemul + ŝ 跨 `p` 缓存 + `aBlock` 合并搬入；证明 **能跑 ≠ 该默认**。  
- 记录 `aBlockQue_` 独立 TQue 相对 scratch 池化的必要性（Exp3-full 教训）。  
- 增量实验表（初版 26600 → 3-full′ 23220）见 [qa/2026-06/2026-06-17-innerproduct-k4一二期路线讨论.md](../../../qa/2026-06/2026-06-17-innerproduct-k4一二期路线讨论.md)。

---

## 快照文件

| 文件 | 说明 |
|------|------|
| [innerproduct_kernel_halfbatch.cpp](innerproduct_kernel_halfbatch.cpp) | 冻结前完整 kernel（含 `#if INNERPRODUCT_HALF_BATCH`） |
| [hat_innerproduct_batch.hpp](hat_innerproduct_batch.hpp) | half ∘ / ŝ 缓存 helpers |
| [innerproduct_tiling_halfbatch.h](innerproduct_tiling_halfbatch.h) | 二期 scratch 偏移布局 |

**勿 fork、勿抄码、勿跑 CI。**
