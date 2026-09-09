# STATUS — RB-D03-decrypt-intt-extract

| 字段 | 值 |
|------|-----|
| 刀 | DRW-D03 · E-D03-INTT-M / G-DG3-INTT-M |
| 状态 | **PASS_CPU + PASS_NPU** |
| 日期 | 2026-09-09 |
| 墙钟 | ~25 min（编码 + CPU + sync_audit） |

## 目标达成

1. `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`；flag **1/3 + 4=GATE**
2. basename 唯一：`dec_intt_extract_custom.cpp`
3. I/O：`ŵ[256]` + `v[256]` → `w[256]` / `m[32]`（ŵ/v 布局对齐 D02/D01）
4. AIV0：Alg.10 InverseNTT + Alg.15 尾 extract（Compress₁ + ByteEncode₁）；禁 NTT∥INTT 同核
5. 写出：UB+DataCopy（X12）

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；m max=0；w max=0；PASS_SYNC+PASS_IO |
| Oracle | host FIPS（T10 `mlkem_intt` + 统一整数 Compress₁；**非 liboqs**） |
| sync_audit | clean（无红线；SYNC-05 假阳性 + SYNC-09 性能） |
| SIM / NPU | **PASS_NPU**（2026-09-09 回填；见云 /tmp/rb-d01-d03-npu.log） |

运营回报：[`../../decrypt-rebuild-ops/tasks/DRW-D03-decrypt-intt-extract/FEEDBACK.md`](../../decrypt-rebuild-ops/tasks/DRW-D03-decrypt-intt-extract/FEEDBACK.md)

## 加压

- NPU×30 lean ok=30 @17:28:19（强制拉起后；旧 chain 曾假衔尾）
