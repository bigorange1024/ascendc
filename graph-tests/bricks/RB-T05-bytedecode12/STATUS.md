# STATUS — RB-T05-bytedecode12

| 字段 | 值 |
|------|-----|
| 刀 | T05 · D-EXP-T05 / G-BD12 |
| 状态 | **PASS**（CPU + `SIM_DIRECT=1` sim） |
| 日期 | 2026-09-08 |
| 墙钟 | ~8 min（编码+双跑） |

## 目标达成

1. `KERNEL_TYPE_AIV_ONLY`，`blockDim=1`；无 MIX / CrossCore
2. I/O：ek 的 ByteEncode₁₂ 载荷 `1536B`（k=4×384）→ `t̂[1024]` int32
3. 薄壳 `#include library/shared/f203_byte_codec/byte_decode12_vec.hpp`，调用 `poly_byte_decode12_scalar_gm`；未从 alg14 搬文件
4. Host golden（Python BE₁₂ round-trip）与设备/CPU 孪生 max=0

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；verify max=0（1024 coeffs） |
| `SIM_DIRECT=1 bash run.sh -r sim …` | SUCCESS；verify max=0；totalTick≈23765 |
| sync_audit | **N/A**（无 CrossCore） |
| 用例根 stray dump | 无（camodel 已收拢） |

运营回报：[`../../encrypt-rebuild-ops/tasks/T05-bytedecode12-probe/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T05-bytedecode12-probe/FEEDBACK.md)
