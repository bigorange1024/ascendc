# STATUS — RB-T13-device-sample-ntt-a

| 字段 | 值 |
|------|-----|
| 刀 | T13 · 设备 Â←SampleNTT(ρ) |
| 状态 | **PASS_SYNC + PASS_IO**（CPU + SIM_DIRECT） |
| 日期 | 2026-09-08 |
| 墙钟 | ~16 min（编码+双跑+审计；含 1 次 SIM BLOCK_DIM 半边失败重跑） |

## 目标达成

1. Host 预喂 ρ[32]（对齐 T07 FIXED_RHO / ek 尾语义）；**禁**预喂最终 Â
2. 设备 MIX：flag **仅 1/3**；AIV0 接活跃 lines3-7 积木 `BuildAHat16ShardWithUb`（`F203_AHAT16_BLOCK_DIM=1`）写出 Â[16,256]
3. CPU/SIM Â max_abs=0；sync_audit 无红线（SYNC-05 薄封装假阳性同 T12）
4. 未抄 alg14/encrypt/frozen；未改 KB/DAG；未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO（kernel≈3s） |
| `SIM_DIRECT=1 bash run.sh -r sim …` | PASS；totalTick≈705084；wall≈355s；无 hang |
| sync_audit | 无红线；SYNC-05(高假阳性)+SYNC-09 性能 |
| 用例根 stray dump | 无（已收拢 `sim_log/`） |

## SIM 踩坑（已修）

首轮 SIM `mism=2048`：`ascendc_library` 未吃到 `target_compile_definitions`，积木默认 `BLOCK_DIM=2` → 只写 poly0–7。  
修复：`sample_ntt_device.hpp` 在 include 前锁定 `F203_AHAT16_BLOCK_DIM=1` + `ascendc_compile_definitions`。

运营回报：[`../../encrypt-rebuild-ops/tasks/T13-device-sample-ntt-a/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T13-device-sample-ntt-a/FEEDBACK.md)
