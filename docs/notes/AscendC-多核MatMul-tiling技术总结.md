# AscendC 多核 int8 MatMul tiling — 技术总结

**读者**：在 Atlas A2 类芯片上做多 AIC Cube 分片的开发者  
**目的**：说明 **单次 launch + 宏分片闭合** 的原理，而非只记某次 `SetFixSplit` 参数  
**规范 PDF**：[融合算子多核tiling策略指南.pdf](../specs/ascendc/融合算子多核tiling策略指南.pdf)  
**讨论**：[qa/2026-06/2026-06-11-…#exp-int8-matmul](../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#exp-int8-matmul-多核-tiling-实验)  
**状态**：探针已归档 `frozen-int8-matmul-cube-128x512x512`（路线关闭，tiling 结论仍可读）

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | 多核 MatMul 的分片数学 |
| §2 | Atlas A2 规模关系 |
| §3 | `SetSingleShape` 闭合网格 |
| §4 | 微分块与对齐约束 |
| §5 | 验证边界（CPU ≠ NPU） |
| §6 | 案例：128×512×512 实验矩阵 |

---

## 1. 分片数学

对 \(C = A B\)，\(A\in\mathbb{Z}^{M\times K}\)，\(B\in\mathbb{Z}^{K\times N}\)，多核目标是把 \(C\) 划分为 **互不重叠的子矩形**，每核一块，**一次 launch** 完成。

记单核负责 \((M_c, N_c)\) 子块，使用核数 \(P\)：

\[
\frac{M}{M_c} \times \frac{N}{N_c} = P \quad\text{（理想闭合时）}
\]

**不变量**：host 配置的 `blockDim`、tiling 生成的 **usedCoreNum**、以及 **实际分块数** 必须一致；否则出现 `tailN` 为负、SIGFPE 或静默错块。

---

## 2. Atlas A2 规模（本仓约定）

- `usedCoreNum = blockDim × 2`（AIV 规模与 AIC 配比）
- int8 微分块最小 tile 常取 \(16\times32\times16\) → `baseM`/`baseN` 为 16/32 的倍数
- `SetFixSplit` 约束 **微分块**；`SetSingleShape` 约束 **宏分片**

二者 **不同层**；只调 FixSplit 不闭合宏网格会失败。

---

## 3. 模式：单次 launch + SingleShape 闭合

| 策略 | 优点 | 失败模式 |
|------|------|----------|
| 多次 CPU launch 绕行 | 易调试 | 非生产路径 |
| 单次 launch，无 SingleShape | 简单 | 核数 > 分块数 → tail 负 |
| **单次 launch + SetSingleShape 闭合** | 与硬件调度一致 | 须手算 \(M/M_c \times N/N_c\) |

**可复用规则**：扩核时先画 **M×N 网格**，再填 `SetSingleShape(singleM, singleN, K)`，验证乘积等于 `usedCoreNum`。

---

## 4. 对齐与 FixSplit

`SetFixSplit(16,32,-1)` 一类配置约束 **块内** 形状；与 **16 AIC** 等宏配置组合时需分别验证。

实验结论（本探针）：`SetFixSplit(32,32,-1)` 在 8 AIC 下失败；`16,32,-1` 可通过 — 说明 **微分块必须与芯片最小 tile 族兼容**，不能仅从 M/N 手算。

---

## 5. 验证方法论

| 层级 | 本探针状态 |
|------|------------|
| CPU 孪生 | 1/4/8/16 AIC 配置 `max_abs_diff=0` |
| SIM / NPU | **未** 作为关闭路线的前置条件 |
| Host tiling | `GetTiling` 失败须 `exit`（当时未做） |

**原则**：tiling 结论可迁移；**性能与实机**须在活跃算子上重验。

---

## 6. 附录：128×512×512 实验摘要

**探针**：`ascendc-tests/frozen/frozen-int8-matmul-cube-128x512x512/`

| 配置 | 结果 |
|------|------|
| 8 AIC + `SetFixSplit(32,32,-1)` | 失败 |
| 8 AIC + `SetFixSplit(16,32,-1)`，无 SingleShape | 通过 |
| 16 AIC，无 SingleShape | 失败（tailN 负） |
| 16 AIC + SingleShape 闭合（三种 M/N 切法） | 通过 |

```bash
cd ascendc-tests/frozen/frozen-int8-matmul-cube-128x512x512
bash run.sh -r cpu -v Ascend910B4
```

**与 NTT 的关系**：本结论 **不** 适用于 NTT MIX 内 Stage2（须 `AicMmad`，见 [NTT-Matmul路线废弃说明.md](NTT-Matmul路线废弃说明.md)）。

---

*原 `docs/research/20260611-exp-int8-matmul多核tiling.md`；2026-06-18 迁入 notes。*
