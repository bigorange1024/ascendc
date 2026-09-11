# STATUS — RB-T26-decaps-device

| 字段 | 值 |
|------|-----|
| 刀 | T26 · D-EXP-T26 |
| 状态 | **PASS_CPU**（PASS_SYNC + PASS_IO）；SIM **skip**（战役 NPU 优先） |
| 日期 | 2026-09-08 |
| 墙钟 | CPU 全链路（编+跑）≈25s；kernel≈6.5s |

## 目标达成

1. Alg.21 Decaps：`dk`+`c` → `K[32]`；五 launch（Decrypt×3 + Encaps prep/compute×2）
2. Flag：Decrypt **1/3**；Reenc **1/3+4**；`BLOCK_DIM=1`；永禁 5/7、SoftSync、Wait 环 SyncAll
3. CPU：`K` ≡ liboqs Decaps；`c'==c`（accept）；TRACE Decrypt+Reenc 全链
4. sync_audit：无红线（SYNC-05 薄封装假阳性同前刀 + SYNC-09 性能）
5. 未抄 alg15/20/21/decrypt/decaps/encrypt/encaps/l18_l19/frozen；未改 KB/DAG；未碰 SSH/NPU

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO（kernel≈6.5s） |
| `SIM_DIRECT=1 … sim` | **skip**（NPU 优先；非本战役门禁） |
| sync_audit | 无红线；SYNC-05 + SYNC-09 |
| 用例根 stray dump | 无（本轮未跑 SIM） |
| golden | `liboqs_kem_ref decaps` |

## 硬锁

- `blockDim=1` / `F203_AHAT16_BLOCK_DIM=1` / `F203_CBD_BLOCK_DIM=1`
- CrossCore：Decrypt 仅 1/3；Reenc 1/3+4；永禁 **5/7**
- Host 禁预喂最终 `K` / `m'`
- out magic `0x543F001A`（T26）

运营回报：[`../../encrypt-rebuild-ops/tasks/T26-decaps-device/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T26-decaps-device/FEEDBACK.md)
