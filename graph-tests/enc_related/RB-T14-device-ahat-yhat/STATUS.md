# STATUS — RB-T14-device-ahat-yhat

| 字段 | 值 |
|------|-----|
| 刀 | T14 · 设备 Â+ŷ 半链拼装 |
| 状态 | **PASS_SYNC + PASS_IO**（CPU + SIM_DIRECT） |
| 日期 | 2026-09-08 |
| 墙钟 | ~27 min（编码+CPU+SIM+审计） |

## 目标达成

1. Host 预喂 ρ[32]（T07 FIXED_RHO）+ y[4×256]（CBD/coins）+ ζ；**禁**预喂最终 Â/ŷ
2. 设备单 launch MIX：flag **仅 1/3**；AIV0 同核顺序 Â←SampleNTT → ŷ←NTT(y)
3. CPU/SIM Â 与 ŷ 双对拍 max_abs=0；sync_audit 无红线（SYNC-05 薄封装假阳性同 T12/T13）
4. `F203_AHAT16_BLOCK_DIM=1` + `ascendc_compile_definitions`；未抄 alg14/encrypt/frozen；未改 KB/DAG；未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO（kernel≈2.8s） |
| `SIM_DIRECT=1 bash run.sh -r sim …` | PASS；totalTick≈956912；wall≈457s；无 hang |
| sync_audit | 无红线；SYNC-05(高假阳性)+SYNC-09 性能 |
| 用例根 stray dump | 无（已收拢 `sim_log/`） |

运营回报：[`../../encrypt-rebuild-ops/tasks/T14-device-ahat-yhat/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T14-device-ahat-yhat/FEEDBACK.md)
