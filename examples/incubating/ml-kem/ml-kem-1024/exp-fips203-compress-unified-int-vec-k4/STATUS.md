# exp-fips203-compress-unified-int-vec-k4

**状态**：完成（全档 d CPU+SIM PASS；默认 **纯 int32 向量 limb** 宽乘）

**规格**：[`exp-fips203-compress-unified-int-vec-k4-实现方案-customspec.pdf`](exp-fips203-compress-unified-int-vec-k4-实现方案-customspec.pdf)

**来源**：自 `ascendc-tests/ml-kem/ml-kem-1024/pass-f203-compress-unified-int-vec-k4` 迁入 `examples/incubating`（2026-07-10）。

## 目标

FIPS 203 §4.2.1 **Compress_d** 统一整数舍入：`C=⌊2^37/q⌋=41285357`，`(C·u + 2^(36-d)) >> (37-d)`。见 [`docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md`](../../docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md)。

## 实现要点

| 项 | 说明 |
|----|------|
| 共享头 | [`library/shared/f203_unified_round/f203_unified_compress_vec.hpp`](../../library/shared/f203_unified_round/f203_unified_compress_vec.hpp) |
| 向量宽乘 | `C = 629·2^16 + 63213`；`Muls(C0)` + `Muls(C1)` + 进位安全 `carry` + `ShiftRight(k-16)` |
| 默认 | `COMPRESS_UNIFIED_INT_VEC=1`；`=0` 为 int64 lane 对照 |
| d | `F203_UNIFIED_ROUND_D` ∈ {1,4,5,10,11} |

## 验收

```bash
bash run.sh -r cpu -v Ascend910B4
F203_UNIFIED_ROUND_D=11 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

生产路径验收：stable Encrypt + PKE round-trip（非本目录单算子）。

## SIM 性能（标量 vs 向量）

| d | 标量 tick | 向量 tick | 提升 |
|---|-----------|-----------|------|
| 1 | 10729 | 3377 | 3.18× |
| 4 | 10758 | 3415 | 3.15× |
| 5 | 10768 | 3382 | 3.18× |
| 10 | 10749 | 3398 | 3.16× |
| 11 | 10744 | 3399 | 3.16× |

golden：`compress_unified_int_ref.c`。
