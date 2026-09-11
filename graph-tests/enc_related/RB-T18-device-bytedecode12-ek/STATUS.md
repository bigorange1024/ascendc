# STATUS — RB-T18-device-bytedecode12-ek

| 字段 | 值 |
|------|-----|
| 刀 | T18 · 设备 ByteDecode₁₂(ek)→t̂ |
| 状态 | **PASS_CPU**（SIM skip；待主控 NPU） |
| 日期 | 2026-09-08 |
| 墙钟 | ~1 min（编码+CPU） |

## 目标达成

1. Host 预喂完整 ek[1568]=BE₁₂(t̂)‖ρ；**禁**预喂最终 t̂
2. 设备 MIX：flag **仅 1/3**；AIV0 `poly_byte_decode12_scalar_gm` → t̂[1024]
3. CPU PASS_SYNC + PASS_IO（max_abs=0）；sync_audit 无红线（SYNC-05 假阳性同前）
4. 未抄 encrypt/alg14/frozen；未改 KB/DAG；未碰 NPU/SSH；契约对齐 T05 勿大段抄

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO |
| SIM | **skip**（NPU 优先） |
| sync_audit | 无红线；SYNC-05 + SYNC-09 |

运营回报：[`../../encrypt-rebuild-ops/tasks/T18-device-bytedecode12-ek/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T18-device-bytedecode12-ek/FEEDBACK.md)
