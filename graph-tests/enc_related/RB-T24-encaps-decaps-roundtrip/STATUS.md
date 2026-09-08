# STATUS — RB-T24-encaps-decaps-roundtrip

| 字段 | 值 |
|------|-----|
| 刀 | T24 · D-EXP-T24 |
| 状态 | **PASS_CPU**（PASS_SYNC + PASS_RT + PASS_IO）；SIM **skip**（战役 NPU 优先） |
| 日期 | 2026-09-08 |
| 墙钟 | CPU 全链路（编+跑）≈24s；kernel≈4.84s |

## 目标达成

1. 设备 Encaps（T23 路径）出 `(c,K)`；`sk`←liboqs KeyGen
2. 权威：`liboqs Decaps(sk,c)→K'` 与 `K` 对拍；缺库 → BLOCKED
3. CPU：PASS_RT（K'≡K）；PASS_IO 中间量全绿；TRACE 含 G_DONE
4. 硬锁 `F203_AHAT16_BLOCK_DIM=1`；Flag 1/3+4；永禁 5/7
5. 未抄 encaps/decaps/alg14/alg21/frozen；未改 KB/DAG；Subagent 未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_RT + PASS_IO（kernel≈4.84s） |
| `SIM_DIRECT=1 … sim` | **skip**（NPU 优先；非本战役门禁） |
| sync_audit | 无红线；SYNC-05 + SYNC-09 |
| 用例根 stray dump | 无（本轮未跑 SIM） |
| cross_backend | `liboqs`（`scripts/liboqs_kem_ref`） |

## 硬锁

- `F203_AHAT16_BLOCK_DIM=1` / `F203_CBD_BLOCK_DIM=1`
- CrossCore flag **1/3 复用 + 4=GATE**；永禁 **5/7**
- Host 禁预喂 coins/K/最终 μ/t̂/y/e/Â/ŷ/u/v/c
- out magic `0x543F0018`（T24）
- 权威往返：liboqs Decaps only（缺库 BLOCKED）

运营回报：[`../../encrypt-rebuild-ops/tasks/T24-encaps-decaps-roundtrip/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T24-encaps-decaps-roundtrip/FEEDBACK.md)
