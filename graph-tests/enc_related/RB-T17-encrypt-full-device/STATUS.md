# STATUS — RB-T17-encrypt-full-device

| 字段 | 值 |
|------|-----|
| 刀 | T17 · D-EXP-T17 |
| 状态 | **PASS_CPU**（PASS_SYNC + PASS_IO）；SIM **skip**（战役 NPU 优先） |
| 日期 | 2026-09-08 |
| 墙钟 | CPU 全链路（编+跑）≈21s；kernel≈4.4s |

## 目标达成

1. 相对 T15：设备 CBD(coins)→y/e₁/e₂，再 Â/ŷ→u,v→pack→c
2. Host 仅 ek/coins/μ/t̂/ζ/γ；**禁**预喂最终 y/e/Â/ŷ/u/v/c
3. CPU：c/u/v/Â/ŷ/yee 对拍 max=0；sync_audit 无红线（SYNC-05 薄封装假阳性同 T15）
4. 修 early-push NPU `a_hat` 半边：`F203_AHAT16_BLOCK_DIM=1` 硬锁 + `ascendc_compile_definitions`
5. 未抄 alg14/encrypt/frozen；未改 KB/DAG；Subagent 未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO（kernel≈4.4s） |
| `SIM_DIRECT=1 … sim` | **skip**（NPU 优先；非本战役门禁） |
| sync_audit | 无红线；SYNC-05 + SYNC-09 |
| 用例根 stray dump | 无（本轮未跑 SIM） |

## 关键锁

- `F203_AHAT16_BLOCK_DIM=1`（sample_ntt / compute 文件头 `#undef`+`#define`；npu_lib `ascendc_compile_definitions`）
- `F203_CBD_BLOCK_DIM=1`
- CrossCore flag **1/3 复用 + 4=GATE**；永禁 **5/7**
- 双 launch 单库；禁 Host 预喂最终 y/e/Â/ŷ/u/v/c

运营回报：[`../../encrypt-rebuild-ops/tasks/T17-encrypt-full-device/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T17-encrypt-full-device/FEEDBACK.md)
