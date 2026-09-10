# STATUS — RB-T23-encaps-liboqs-cross

| 字段 | 值 |
|------|-----|
| 刀 | T23 · D-EXP-T23 |
| 状态 | **PASS_CPU**（PASS_SYNC + PASS_CROSS + PASS_IO）；SIM **skip**（战役 NPU 优先） |
| 日期 | 2026-09-08 |
| 墙钟 | CPU 全链路（编+跑）≈22s；kernel≈4.62s |

## 目标达成

1. 相对 T22：ek←liboqs KeyGen；golden_c/K←liboqs Encaps（同固定 m）；设备 Encaps 交叉对拍
2. 缺 liboqs/仓内 ref → gen_data 写 BLOCKED（exit 2），禁止 python 冒充权威
3. CPU：PASS_CROSS（c/K ≡ liboqs）；PASS_IO 中间量全绿；TRACE 含 G_DONE
4. 硬锁 `F203_AHAT16_BLOCK_DIM=1`；Flag 1/3+4；永禁 5/7
5. 未抄 encaps/alg14/alg20/encrypt/frozen；未改 KB/DAG；Subagent 未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_CROSS + PASS_IO（kernel≈4.62s） |
| `SIM_DIRECT=1 … sim` | **skip**（NPU 优先；非本战役门禁） |
| sync_audit | 无红线；SYNC-05 + SYNC-09 |
| 用例根 stray dump | 无（本轮未跑 SIM） |
| cross_backend | `liboqs`（`scripts/liboqs_kem_ref`） |

## 关键锁

- `F203_AHAT16_BLOCK_DIM=1` / `F203_CBD_BLOCK_DIM=1`
- CrossCore flag **1/3 复用 + 4=GATE**；永禁 **5/7**
- Host 禁预喂 coins/K/最终 μ/t̂/y/e/Â/ŷ/u/v/c
- out magic `0x543F0017`（T23）
- 权威 golden：liboqs only（缺库 BLOCKED）

运营回报：[`../../encrypt-rebuild-ops/tasks/T23-encaps-liboqs-cross/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T23-encaps-liboqs-cross/FEEDBACK.md)

## 性能（NPU · 设备真值 · 2026-09-10）

登记见 [`qa/active_npu_perf_summary.md`](../../../qa/active_npu_perf_summary.md)（对照测；非 launch 压缩刀）。

| 项 | 值 |
|----|-----|
| Host launches | 2 |
| Σ Task Duration | **1283.54 µs**（1.284 ms） |
| `prep_custom` | 63.44 µs |
| `compute_custom` | 1220.10 µs |
| 采集 | ops-profiling · `docs/perf/round_001/` |
