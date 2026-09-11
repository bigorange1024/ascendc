# STATUS — RB-D02-decrypt-ntt-dot

| 字段 | 值 |
|------|-----|
| 刀 | DRW-D02 · E-D02-NTT-DOT / G-DG2-NTT-DOT |
| 状态 | **PASS_CPU + PASS_NPU** |
| 日期 | 2026-09-09 |
| 墙钟 | ~15 min（编码 + CPU + sync_audit） |

## 目标达成

1. `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`；flag **1/3 + 4=GATE**
2. basename 唯一：`dec_ntt_dot_custom.cpp`
3. I/O：`u[1024]` + `ŝ[1024]` → `û[1024]` / `ŵ[256]`（int32）
4. AIV0：Alg.9 ForwardNTT + Alg.11 ΣMultiplyNTTs；poly-batch 整 poly；禁 Gather/limbsplit
5. 写出：UB+DataCopy（X12）

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；û/ŵ max=0；PASS_SYNC+PASS_IO |
| Oracle | host FIPS（T10 `topology_math`；**非 liboqs**） |
| sync_audit | clean（无红线；SYNC-05 假阳性 + SYNC-09 性能） |
| SIM / NPU | **PASS_NPU**（2026-09-09 回填；见云 /tmp/rb-d01-d03-npu.log） |

运营回报：[`../../decrypt-rebuild-ops/tasks/DRW-D02-decrypt-ntt-dot/FEEDBACK.md`](../../decrypt-rebuild-ops/tasks/DRW-D02-decrypt-ntt-dot/FEEDBACK.md)

## 加压

- NPU×30 lean ok=30 @17:17:56
