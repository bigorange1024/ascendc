# frozen-fix-f203-alg13-16171819-k4

> **已冻结**（2026-06-12）：迁入 `ascendc-tests/frozen/`。继任：[fix-f203-2s1e-alg13-16171820-k4](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) — **禁止抄码**

Alg.13 行 **16–17–18–19**（k=4）：Tag5T NTT → NTT 域内积 → **AIV 嵌 C `ByteEncode₁₂`**（`ek_polyvec` / `sk_polyvec`）。  
行 16–17 NTT 当前为 limbsplit-s123 limb 面对半，**待基于 poly-batch 重建**（[MLKEM-NTT-实现总结](../../../docs/notes/MLKEM-NTT-实现总结.md)、[T11](../../../qa/TODO.md)）。

| 阶段 | CPU | SIM |
|------|-----|-----|
| 全链路 mixPass=0 | ✓ | ✓ |
| 仅行 19 mixPass=5 | — | — |

**CPU 行 19 同步**：`ICPU_RUN_KF` 对 AIC/AIV_0/AIV_1 分进程 launch，进程间 `volatile` 不共享；行 18 完成计数写在 `ws + LINE18_AIV_SYNC`（`wssize_alloc = wssize + 8`），第二次 AIV launch 后再 `ByteEncode₁₂`。

```bash
cd ascendc-tests/frozen/frozen-fix-f203-alg13-16171819-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

上游：[frozen-fix-f203-alg13-161718-k4](../frozen-fix-f203-alg13-161718-k4/)（行 16–18）  
计划：[ALG13_16171819_PLAN.md](ALG13_16171819_PLAN.md)
