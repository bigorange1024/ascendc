# docs/specs/ascendc — AscendC 规格与实施说明

Atlas A2 / KernelLaunch 相关的**可复用规范**（与 `qa/` 讨论纪要、`examples/incubating/exp-*` 实验互链）。

| 文件 | 何时阅读 |
|------|----------|
| [融合算子多核tiling策略指南.pdf](融合算子多核tiling策略指南.pdf) | 多 AIC MatMul / 融合算子 Host tiling、`SetDim`/`SetSingleShape`/`SetFixSplit` 闭合条件 |
| [融合算子多核tiling策略指南.tex](融合算子多核tiling策略指南.tex) | 指南源码；改 `.tex` 后须 `xelatex` 两遍生成 PDF，成功后删中间文件（见 Rule「LaTeX / PDF」） |

**实验依据**（路线已冻结）：[`frozen-int8-matmul-cube-128x512x512`](../../../ascendc-tests/frozen/frozen-int8-matmul-cube-128x512x512/) · 纪要 [`qa/2026-06/2026-06-11-…`](../../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#exp-int8-matmul-多核-tiling-实验)

---

## 维护

新增 AscendC 规范 → 在本表登记；大改指南时同步更新对应 `qa/` 实验纪要。
