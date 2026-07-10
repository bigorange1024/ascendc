# pass-f203-compress-unified-int-vec-k4 — 实现计划

## 背景

用户推导的统一整数 Compress（`C=41285357`，`y=2^(37-d)`）已在 Host 穷举验证 0 mismatch；需 AscendC 探针落地。详见技术总结与 `qa/2026-07-10` §14。

## 设计决策

1. **公式锁定**：`(C·u + 2^(36-d)) >> (37-d)`，不按 d 换 magic、不用 float。
2. **宽乘路径**：`C=629·2^16+63213`；两次 `Muls` + 进位安全 `carry` + `ShiftRight(k-16)`（纯 int32 向量，见 `f203_unified_compress_vec.hpp`）。
3. **共享头**：`library/shared/f203_unified_round/`，供 Encrypt tail 后续迁移。
4. **验收**：`run.sh` + golden `compress_unified_int_ref.c`；d∈{1,4,5,10,11}。

## 文件

| 路径 | 作用 |
|------|------|
| `compress_unified_int_custom.cpp` | AIV kernel 壳 |
| `compress_unified_int_ref.c` | golden |
| `scripts/gen_data.py` / `verify_result.py` | 数据与对拍 |

## 后续

- Encrypt tail 从 `pass-f203-compress-d-vec-k4` 迁移到本公式（需评估 int64 lane 与 tick）。
- 若 CANN 后续提供宽乘向量原语，可替换 lane 循环而保持公式不变。
