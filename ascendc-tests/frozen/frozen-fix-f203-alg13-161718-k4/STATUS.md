# frozen-fix-f203-alg13-161718-k4

> **已冻结**（2026-06-12）：迁入 `ascendc-tests/frozen/`；行 16–17 仍 limbsplit，**勿 fork**。  
> 继任活跃探针：[fix-f203-2s1e-alg13-16171820-k4](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) · [frozen/INDEX.md](../INDEX.md)

**状态**：AscendC 行 16–17–18 全链路 **CPU + SIM 已验收**（冻结前）

## 范围

- **行 16–17**：Tag5T MIX NTT（`se` [8,256] → `dst` = ŝ‖ê）；当前仍用 **limbsplit-s123 limb 面对半**，**待基于 poly-batch 重建**（[T11](../../../qa/TODO.md)）
- **行 18**：AIV `MultiplyNTTs` 内积 + ê + mod q → `t_hat` [4,256]
- **C 仅 golden**：`hat_inner_product_ref.c` 供 `gen_data.py` 生成 `golden/t_hat.bin`

## 验收

```bash
cd ascendc-tests/frozen/frozen-fix-f203-alg13-161718-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 KERNEL_COMPUTE_BUDGET_SEC=120 bash run.sh -r sim -v Ascend910B4
```

行 18 内积/同步调试见 [HAT_DEBUG_REPORT.md](HAT_DEBUG_REPORT.md)

## 文档

- [MLKEM-NTT-实现总结](../../docs/notes/MLKEM-NTT-实现总结.md) — NTT 定稿 poly-batch；本探针行 16–17 待迁
- [ALG13_161718_PLAN.md](ALG13_161718_PLAN.md)
- [DataCopy 知识库](../../docs/notes/ascendc-DataCopy与数据搬运知识库.md)
- [TQue 知识库](../../docs/notes/ascendc-TQue与Pipe框架知识库.md)
- [qa 2026-06-12 讨论](../../qa/2026-06/2026-06-12-F203-alg13行18-TQue与模运算讨论.md)
