# 路线与算子场景纪要（非强制加载）

**性质**：本仓某次预研/探针的**路线决策与踩坑记录**，不是 AscendC 平台通则。  
**加载方式**：仅当用户指定的 `*-customspec.*` 或任务明确 `@` 本文件时再读；**不得**替代 [../SKILL.md](../SKILL.md)。

通用 AscendC 技术（标量/向量/矩阵/搬运/同步）一律以 **SKILL.md** 为准。

---

## 1. F203 / Kyber 片段与仓库目标（背景）

| 范围 | 说明 |
|------|------|
| FIPS 203 | ML-KEM 全算法在 AscendC 上落地（规划见 `qa/2026-06/2026-06-08-Rule-Skill落地与FIPS203-204终极目标.md`） |
| 当前片段 | NTT、encode、matmul、mod 等为子块；单块通过 ≠ 算法完成 |

---

## 2. MIX 路线对照（merged_kyber vs Matmul — **NTT 内 Matmul 路线已废弃**）

**当前本仓 F203 批 NTT 探针/exp 采用**：[`pass-merged-kyber-mix-ntt256`](../../../ascendc-tests/pass-merged-kyber-mix-ntt256/) 手写 FSM（`CrossCore*` + `AivSplit` → `AicMmad`×2 → `AivMerge` + Barrett；原 `thirdparty/merged_kyber` 授权迁入）。**CPU+SIM 已走通**。

**NTT 内使用 `Matmul<>` 的路线：废弃中止、冻结**（2026-06-11）。`examples/frozen/frozen-exp-*` 与 `ascendc-tests/frozen/frozen-*` **均已关闭** — 只读各 `FROZEN.md`，**禁止抄实现、禁止抄路线**。纪要 [qa/2026-06/…#NTT-Matmul](../../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#ntt-matmul路线废弃冻结)。

**勿再**将 Kyber NTT Stage2 接到 `Matmul<>` 或 LeakyRelu 融合模板。与 NTT **无关**的通用 Cube 探针若新建，须单独 customspec，且不得沿用已归档路径名。

| 维度 | NTT 内 `Matmul<>`（**废弃**） | merged_kyber FSM（**主路径**） |
|------|-------------------------------|--------------------------------|
| 状态 | 冻结；s12 SIM 未闭合 | poly8/block-s123、exp k4 CPU+SIM ✓ |
| Stage2 | 高阶 `Matmul<>`；`[16,512]` 与 limb6 紧凑态易错位 | `AicMmad` + `Nd2NzParams`，`mRows=2×kPolys` |
| Stage3 | RouteA 从 `[16,512]` Gather | 紧凑 `A0/A1` `[2k,256]`，行 `2p`/`2p+1` + Merge |

**另一条 NTT 实现**不必沿用上表；以各自 customspec 的张量契约为准。

---

## 3. 批处理 NTT（k≥2）— merged_kyber 路线硬约束

仅适用于采用 **merged_kyber 批处理壳** 的用例（如已冻结 `frozen/frozen-fix-merged-kyber-ntt256-limb6-poly8-s123`、`exp-sepolyvec8-ntt-k8`）：

1. 每个 AIV 算子类仅一个 `TPipe`；`Init()` 里只 `InitBuffer` 一次。
2. 禁止 `for (p) { AivSplit split; … }` — `AllocEventID` 耗尽 / 挂死。
3. `tileLength = kPolys × (n/2)`；一趟 `split_vec`；`for (p)` 仅 GM CopyIn/Out。
4. Stage2：`AicMmad(mRows,256,256)`，`mRows = 2×kPolys`。
5. Merge：从紧凑 `A0/A1` 读行 **`2p` / `2p+1`**；槽 2/3 置零。
6. batch 版 `aiv_func.hpp` 须在探针/exp **本地**维护；[`pass-merged-kyber-mix-ntt256/aiv_func.hpp`](../../../ascendc-tests/pass-merged-kyber-mix-ntt256/aiv_func.hpp) 仍为单 poly API；CMake `TEST_ROOT` 在 include 最前。

---

## 4. GM 中间态布局（limb6 / RouteA — 本路线张量契约）

逻辑上可能都涉及「多行系数表」，但 **GM 寻址语义不同**：

```text
【A】ND 紧凑 limb6（merged_kyber Stage2 产出 A0/A1）
     [2k, 256]：poly p 占行 2p、2p+1

【B】F203 RouteA 宽表（ONNX 语义）
     [16, 512]：Gather 按偶/奇列切片 — 与【A】不是同一套地址

【C】单 poly 参考（k=1）
     A0/A1 各 [4, 256]：行 0,1 有效，2,3 为零

【D】Tag5T mat_c 竖堆（fix-f203）
     [32, 256]：C_lo 行 0..15、C_hi 行 16..31

【E】Tag5T poly-batch Stage3（本仓 MLKEM **强制**）
     AIV0 读 C_lo[0:8)+C_hi[16:24)；AIV1 读 C_lo[8:16)+C_hi[24:32)
     每 AIV 握 kPolysPerAiv 个 **完整 poly** 的 hi+lo — 禁止单 poly hi/lo 分属两 AIV
```

limb6 批 NTT（merged_kyber 路线）用 **【A】**；历史 RouteA 宽表实验用 **【B】**。  
**ML-KEM / FIPS 203 Tag5T NTT** 活跃探针：**`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`**（平面 mat_c；**NTT S1–S3 无 Gather**；UB 行 18–20）。标量对照组 **`frozen-fix-f203-2s1e-alg13-16171820-k4`** 已归档。`Gather` 禁令**不覆盖** ByteEncode 等非 NTT 步骤（见指南 §5.1）。

详见 [docs/notes/MLKEM-NTT-向量与标量实现指南.md](../../../docs/notes/MLKEM-NTT-向量与标量实现指南.md)。

---

## 5. Tag5T poly-batch 硬约束（MLKEM NTT）

适用于 **`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`** 及后续 MLKEM 集成（**勿**从 frozen 探针抄实现）：

1. **Stage1**：`AivSplitPolyBatch` — 按 **poly 批**切分；`lo = v - hi*64`（非 `And(v,63)`）。
2. **Stage2**：`2× AicMmad` → **平面** `mat_c`（非竖堆 + Gather S3）。
3. **Stage3**：平面 merge + mod；每 AIV 完整 poly hi+lo；**NTT 段无 Gather**、无 AIV↔AIV GM。
4. **行 18–20**：`Aiv2s1eUbPipeline` UB 融合；host 1s+1e；禁止 `SHAT_PEER` / se_pair。

数学契约见 [MLKEM-NTT-实现总结.md](../../../docs/notes/MLKEM-NTT-实现总结.md) §1–§2。

---

## 6. 探针迁移（历史）

merged_kyber / polybatch-s123 / sepair 等已迁入 `frozen/` — **路线关闭**，不作为迁移源。新工作从 **2s1e** fork。

---

## 7. 延伸阅读（场景纪要）

| 主题 | 路径 |
|------|------|
| **MLKEM NTT 实现总结（定稿）** | `docs/notes/MLKEM-NTT-实现总结.md` |
| poly-batch 架构定稿 | `qa/2026-06/2026-06-12-F203-alg13行18-TQue与模运算讨论.md` §7 |
| MIX 策略总览 | `docs/notes/F203-merged-kyber-MIX路线技术总结.md` |
| 批 NTT | `docs/notes/merged-kyber-poly-batch-NTT技术总结.md` |
| 当日讨论 | `qa/2026-06/2026-06-10-F203-MIX-merged_kyber路线与limb6.md` |
