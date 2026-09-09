# STATUS — RB-D01-decrypt-prep

| 字段 | 值 |
|------|-----|
| 刀 | DRW-D01 · E-D01-PREP / G-DG1-PREP |
| 状态 | **PASS_CPU + PASS_NPU** |
| 日期 | 2026-09-09 |
| 墙钟 | ~20 min（编码 + CPU） |

## 目标达成

1. `KERNEL_TYPE_AIV_ONLY`，`blockDim=1`；无 MIX / CrossCore
2. basename 唯一：`dec_prep_custom.cpp`
3. I/O：`dk_pke[1536]` + `c[1568]` → `ŝ[1024]` / `u[1024]` / `v[256]`（int32）
4. BD₁₂ → shared `poly_byte_decode12_scalar_gm`；d=5/11+Decompress → 本地 helpers；写出 DataCopy（X12）

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；ŝ/u/v max=0 |
| Oracle | host Python FIPS 契约（**非 liboqs**） |
| sync_audit | clean（无 CrossCore 红线；SYNC-09 性能提示） |
| SIM / NPU | **PASS_NPU**（2026-09-09 回填；见云 /tmp/rb-d01-d03-npu.log） |

运营回报：[`../../decrypt-rebuild-ops/tasks/DRW-D01-decrypt-prep/FEEDBACK.md`](../../decrypt-rebuild-ops/tasks/DRW-D01-decrypt-prep/FEEDBACK.md)

## 加压

- NPU×30 lean ok=30 @17:15:57
