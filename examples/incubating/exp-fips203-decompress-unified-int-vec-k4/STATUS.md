# exp-fips203-decompress-unified-int-vec-k4

**状态**：完成（全档 d CPU PASS；SIM 已验 d=4/11）

**规格**：[`exp-fips203-decompress-unified-int-vec-k4-实现方案-customspec.pdf`](exp-fips203-decompress-unified-int-vec-k4-实现方案-customspec.pdf)

**来源**：自 `ascendc-tests/pass-f203-decompress-unified-int-vec-k4` 迁入 `examples/incubating`（2026-07-10）。

## 目标

FIPS 203 §4.2.1 **Decompress_d**：`(c·q + 2^(d-1)) >> d`，**全档 int32 向量**。见 [`docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md`](../../docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md)。

## 实现要点

| 项 | 说明 |
|----|------|
| 共享头 | [`library/shared/f203_unified_round/f203_unified_decompress_vec.hpp`](../../library/shared/f203_unified_round/f203_unified_decompress_vec.hpp) |
| 默认 | `DECOMPRESS_UNIFIED_INT_VEC=1` |
| d | `F203_UNIFIED_ROUND_D` ∈ {1,4,5,10,11}（**含 d=1**） |

## 验收

```bash
bash run.sh -r cpu -v Ascend910B4
F203_UNIFIED_ROUND_D=11 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

生产路径验收：stable Decrypt + PKE round-trip。

| d | CPU | SIM |
|---|-----|-----|
| 1 | ✓ | — |
| 4 | ✓ | ✓ (~3215 tick) |
| 5 | ✓ | — |
| 10 | ✓ | — |
| 11 | ✓ | ✓ |

golden：`decompress_unified_int_ref.c`。
