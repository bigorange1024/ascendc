# STATUS — RB-T19-encrypt-ek-decode-full

| 字段 | 值 |
|------|-----|
| 刀 | T19 · D-EXP-T19 |
| 状态 | **PASS_CPU**（PASS_SYNC + PASS_IO）；SIM **skip**（战役 NPU 优先） |
| 日期 | 2026-09-08 |
| 墙钟 | CPU 全链路（编+跑）≈21s；kernel≈4.65s |

## 目标达成

1. 相对 T17：设备 ByteDecode₁₂(ek)→t̂，Host **不再**预喂 t̂
2. 再 CBD+Â/ŷ→u,v→pack→c；Host 仅 ek/coins/μ/ζ/γ/mat
3. CPU：c/t̂/u/v/Â/ŷ/yee/ρ 对拍 max=0；sync_audit 无红线（SYNC-05 薄封装假阳性同 T17）
4. 硬锁 `F203_AHAT16_BLOCK_DIM=1`（文件头 + npu_lib `ascendc_compile_definitions`）
5. 未抄 alg14/encrypt/frozen；未改 KB/DAG；Subagent 未碰 NPU/SSH

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO（kernel≈4.65s） |
| `SIM_DIRECT=1 … sim` | **skip**（NPU 优先；非本战役门禁） |
| sync_audit | 无红线；SYNC-05 + SYNC-09 |
| 用例根 stray dump | 无（本轮未跑 SIM） |

## 关键锁

- `F203_AHAT16_BLOCK_DIM=1`
- `F203_CBD_BLOCK_DIM=1`
- CrossCore flag **1/3 复用 + 4=GATE**；永禁 **5/7**
- 双 launch 单库；禁 Host 预喂最终 t̂/y/e/Â/ŷ/u/v/c
- ek=BE₁₂(t̂)‖ρ；设备 Decode₁₂→OFF_T_HAT

运营回报：[`../../encrypt-rebuild-ops/tasks/T19-encrypt-ek-decode-full/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T19-encrypt-ek-decode-full/FEEDBACK.md)
