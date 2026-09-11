# STATUS — RB-D04-decaps-G

| 字段 | 值 |
|------|-----|
| 刀 | DRW-K01 · E-K01-G / **G-DG4-G** |
| 状态 | **PASS_CPU + PASS_NPU** |
| 日期 | 2026-09-09 |
| 墙钟 | CPU kernel≈1.05s；NPU `wall_sec=3.258`（预算 180） |

## 目标达成

1. `KERNEL_TYPE_AIV_ONLY`，`blockDim=1`；无 MIX / CrossCore
2. basename：`decaps_g_custom.cpp`
3. I/O：`m'[32]` + `h[32]`（dk 切片语义）→ `K'[32]` / `r'[32]`
4. X12 DataCopy；Host 不预喂 K'/r'

## 验收证据

| 项 | 结果 |
|----|------|
| CPU Ascend910B4 | K'/r' max=0；oracle=hashlib.sha3_512 |
| NPU Ascend910B3 | 同上 max=0；`wall_sec=3.258` |
| sync_audit | clean（仅 SYNC-09） |

运营：[`../../decrypt-rebuild-ops/tasks/DRW-K01-decaps-G/FEEDBACK.md`](../../decrypt-rebuild-ops/tasks/DRW-K01-decaps-G/FEEDBACK.md)
