# 2026-06-17 — polyvec 内积探针（2×2×1）：一期 / 二期路线收敛与交付决策

**记录时间**：2026-06-17

**探针**：[`pass-fix-f203-alg11-12-innerproduct-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg11-12-innerproduct-k4/)

**知识库**：[F203-innerproduct-k4-技术总结.md](../../docs/notes/F203-innerproduct-k4-技术总结.md)

---

## 结论摘要

1. **交付**：活跃探针 **仅一期全 poly**（`ProcessFullPoly`，`p→j`，4× `compute_on_ub`）。
2. **二期冻结**（同日晚）：`ProcessHalfBatch` 迁入 [`frozen/.../halfbatch`](../../ascendc-tests/frozen/frozen-fix-f203-alg11-12-innerproduct-k4-halfbatch/) — [FROZEN.md](../../ascendc-tests/frozen/frozen-fix-f203-alg11-12-innerproduct-k4-halfbatch/FROZEN.md)；**禁止再参考或抄码**。
3. **SIM 打平**（`ALG11_VEC_OPTS=1`）：一期 **~23248** vs 二期 **~23220** tick（噪声内）；相对二期初版 ~26600 约 −13%。
4. **冻结主因**：小形状 **无性能收益** + **复杂度高** + 4×4×1 **未验证**。
5. **文档**：本 note、PLAN、STATUS、frozen `FROZEN.md`。

---

## 1. 当日里程碑

| 时段 | 内容 |
|------|------|
| 上午 | 二期增量实验 1–4 收敛（`j→half→p`、ŝ 缓存、`gather_f_half_pairs`、3-lite、`OPTS=1`） |
| 下午 | Exp3-full scratch `aBlock` 失败定位（重叠）；回滚 |
| **晚** | **Exp3-full′**：`aBlockQue_` 独立 TQue；CPU/SIM ✓；与一期 SIM 对照 |
| **决策** | 默认改回一期；二期保留；`CMake INNERPRODUCT_HALF_BATCH=0` |
| 文档 | note + PLAN + STATUS + 本纪要 |

---

## 2. 问题背景

- **目标**：在单 AIV 上实现 `t̂[p] = mod_q(Σ_j Â[p,j]∘ŝ[j])`，无 ê，对齐 `hat_inner_product_ref.c`。
- **两路线**：
  - **一期**：直接复用 Alg11 toy 全 poly `compute_on_ub`（参照插针）。
  - **二期**：half 批处理（128 系数 / `pairCount=64`），探索 GM 合并搬入与 ŝ 跨 `p` 缓存（为更大 `P_OUT` 铺路）。

---

## 3. 技术路线对照

### 3.1 一期（默认）

```text
for p:
  acc ← 0
  for j:
    TQue ← Â[p,j], ŝ[j]  (各 256)
    acc += compute_on_ub(...)
  mod_q → outLine[p]
一次 DataCopy → t_hat
```

- scratch `5×N`；无 `aBlockQue_`。
- 代码简单、UB 小、与 multiplyntts-k4 心智模型一致。

### 3.2 二期（`INNERPRODUCT_HALF_BATCH=1`）

```text
for j:
  TQue ← ŝ[j] 256; aBlockQue ← Â[j,*] 512
  for half in {0,1}:
    cache_s_half_blanes(ŝ half)
    for p:
      fHalf ← aBlock[p*N+subOff]   // UB 切片
      basemul_half_cached_s → prod
      累加 outLine[p][half]
mod_q × p → 一次写 GM
```

- 核心增量：`cache_s_half_blanes` + `basemul_half_cached_s` + `gather_f_half_pairs`。
- **3-full 关键**：`aBlock` 必须 **独立 TQue**，不能塞进 scratch 池。

---

## 4. 增量实验（命令证据）

环境：`ALG11_VEC_OPTS=1`，`ALG11_MEM_OPS=1`，`Ascend910B4`，`bash run.sh -r sim`。

| # | 改动 | verify | Total tick |
|---|------|--------|------------|
| 初版 | `half→p→j`，无 ŝ 缓存 | ✓ | ~26600 |
| 1+2 | `gather_f_half_pairs` + `j→half→p` + ŝ cache | ✓ | ~27514→优化后 ~25527 |
| 3-lite | 每 `j` 搬 ŝ 全 256 | ✓ | ~25527 |
| 3-full | scratch `aBlock` | **✗** | ~24752（错） |
| **3-full′** | `aBlockQue_` | ✓ | **~23220** |
| 4 | `ALG11_VEC_OPTS=1` 默认 | ✓ | ~24242（3-lite 时） |
| **一期** | `INNERPRODUCT_HALF_BATCH=0` | ✓ | **~23248** |

```bash
# 一期（默认）
bash run.sh -r sim -v Ascend910B4

# 二期
INNERPRODUCT_HALF_BATCH=1 bash run.sh -r sim -v Ascend910B4
```

---

## 5. 踩坑复盘（当日闭环）

| 坑 | 教训 |
|----|------|
| scratch `aBlock` @768 + `sHalfBuf` @512 | `aBlock[1]` 从 1024 起，与 `kOffSHalf` **重叠** → 写 ŝ 破坏 Â |
| half ∘ 无 TQue | 必须 Alloc→EnQue→DeQue |
| `bind_vec_ws(64)` | interleave ROM 要 **128 int 间距** |
| `gather(f,f)` | f 侧只能用 `gather_f_half_pairs` |
| `a_col` 连续 `2×128` | 列主序下行间 half **不连续** |
| 一期 j-merge 试做 | `p→j` 与 `j→p`+aBlock 在 2×2×1 上 **tick 无优势** |

详见 note [§6](../../docs/notes/F203-innerproduct-k4-技术总结.md#6-附录)。

---

## 6. 交付决策 → 二期冻结（2×2×1）

| 维度 | 选择 |
|------|------|
| 活跃交付 | **仅一期** `ProcessFullPoly` |
| 二期 | **冻结** → `frozen/frozen-fix-f203-alg11-12-innerproduct-k4-halfbatch/` |
| 性能 | 打平（~23.2k tick）→ **不值得维护双路线** |
| 扩 4×4×1 | **新开 spike**；读 `FROZEN.md`，勿 fork 快照 |

---

## 7. 与 Alg11 单 tile 探针的差异

| 项 | multiplyntts-k4 | innerproduct-k4 |
|----|-----------------|-----------------|
| 默认 `ALG11_VEC_OPTS` | `0` 更快 | **`1`**（本探针 CMake 默认） |
| 热路径 | 1× basemul | 4× basemul + ∑ + mod |
| 主要优化方向 | ROM DataCopy（MEM_OPS） | 循环顺序、ŝ 缓存、Â 块搬入 |

---

## 8. 下一步

| 优先级 | 项 |
|--------|-----|
| P1 | 接入 alg13-vec 行 18 时用 **一期内积** 或 multiplyntts `compute_on_ub` |
| P2 | 扩 4×4×1：**新 spike**（勿恢复 `INNERPRODUCT_HALF_BATCH`） |
| P3 | 三期 `+ê` |

---

## 9. 二期冻结实施（同日追加）

| 动作 | 说明 |
|------|------|
| 快照 | `innerproduct_kernel_halfbatch.cpp`、`hat_innerproduct_batch.hpp`、`innerproduct_tiling_halfbatch.h` → frozen |
| 活跃探针 | 删除 `ProcessHalfBatch`、`aBlockQue_`、`INNERPRODUCT_HALF_BATCH`、二期 scratch 布局 |
| 注释 | `innerproduct_kernel.cpp` 文件头标明冻结路径与 GM/scratch 契约 |
| 索引 | `frozen/INDEX.md`、`ascendc-tests/INDEX.md`、note、PLAN、STATUS |

**冻结原因一句话**：2×2×1 下二期 **无 SIM 收益**、**工程复杂度高**、**未在大形状验证** — 不符合活跃树维护成本。

---

## 10. 关联链接

- [FROZEN.md](../../ascendc-tests/frozen/frozen-fix-f203-alg11-12-innerproduct-k4-halfbatch/FROZEN.md)
- [STATUS.md](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg11-12-innerproduct-k4/STATUS.md)
- [2026-06-16 Alg11/12 向量化纪要](2026-06-16-Alg11-12向量化与微优化A-B.md)
- [2026-06-12 alg13 行18 TQue 纪要](2026-06-12-F203-alg13行18-TQue与模运算讨论.md)

---

## 11. 后续（2026-06-18 更新）

4×4×1 单用例曾误用 `a_col` 列主序；已统一为 alg13 **`a_hat` 行主序**，全量 / 半行探针 CPU+SIM PASS。详见 [2026-06-18-内积布局与NTT内积UB融合讨论.md](2026-06-18-内积布局与NTT内积UB融合讨论.md) §1。
