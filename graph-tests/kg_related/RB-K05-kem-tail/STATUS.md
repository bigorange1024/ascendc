# STATUS — RB-K05-kem-tail

| 字段 | 值 |
|------|-----|
| 刀 | KGR-K01 · L3 `kg_kem_tail` |
| 状态 | **PASS_CPU**（npu: wait_npu） |
| 日期 | 2026-09-09 |
| 墙钟 | CPU kernel≈1.05s（预算 120s） |

## 目标达成

1. `KERNEL_TYPE_AIV_ONLY`，`blockDim=1`；无 MIX / CrossCore
2. basename：`kg_kem_tail_custom.cpp`
3. I/O：`ek[1568]` + `dk_pke[1536]` + `seed_d` → `H(ek)[32]` / `z[32]` / `dk_kem[3168]`
4. 布局：`dk_kem = dk_pke ‖ ek ‖ H(ek) ‖ z`；`SEED_D=20260619`；z 域分离对齐 `liboqs_kem_fixture`
5. X12 DataCopy；Host 不预喂 h/z/dk_kem
6. 上游：RB-K04 `ek_pke`/`dk_pke`（未改 P01–P04）

## 验收证据

| 项 | 结果 |
|----|------|
| CPU Ascend910B4 | H/z/dk_kem **max=0**；oracle=host SHA3-256 + 拼接 |
| NPU | **wait_npu**（云机关机；本刀未 SSH/-r npu） |
| sync_audit | clean（仅 SYNC-09 性能） |

运营：[`../../keygen-rebuild-ops/tasks/KGR-K01-kem-tail/FEEDBACK.md`](../../keygen-rebuild-ops/tasks/KGR-K01-kem-tail/FEEDBACK.md)
