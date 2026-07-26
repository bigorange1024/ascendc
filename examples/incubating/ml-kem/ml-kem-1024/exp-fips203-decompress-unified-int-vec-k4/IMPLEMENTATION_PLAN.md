# pass-f203-decompress-unified-int-vec-k4 — 实现计划

## 背景

Decompress 统一整数式 `(c·q + 2^(d-1)) >> d` 与现仓 `pass-f203-decompress-d-vec-k4` 同形，但纳入 **d=1** 并与 Compress 统一探针共用 `F203_UNIFIED_ROUND_D`。

## 设计决策

1. **全 int32 向量**：`Muls(q) → Adds(bias) → ShiftRight(d)`，与旧探针相同 ISA 形态。
2. **共享头**：`library/shared/f203_unified_round/f203_unified_decompress_vec.hpp`。
3. **验收**：d∈{1,4,5,10,11}，CPU + SIM（代表档 d=4/11）。

## 后续

Decrypt 链可切换本共享头，与 ByteDecode 标量 unpack 配对（见 ByteEncode 选型笔记 §4）。
