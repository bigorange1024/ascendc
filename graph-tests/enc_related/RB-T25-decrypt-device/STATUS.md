# STATUS — RB-T25-decrypt-device

| 字段 | 值 |
|------|-----|
| 刀 | T25 · D-EXP-T25 |
| 状态 | **PASS_CPU**（复验）；NPU 首轮 FAIL→已修 GM 路径，**待主控复测**；SIM skip |
| 日期 | 2026-09-08 |
| 墙钟 | CPU 修后≈17s；kernel≈0.99s |

## 目标达成

1. Alg.15 Decrypt：`dk_pke`+`c` → `m[32]`；三 launch（prep AIV → NTT MIX → INTT+extract）
2. Flag **仅 1/3**；`BLOCK_DIM=1`；永禁 5/7、SoftSync、Wait 环 SyncAll
3. CPU：`m`≡liboqs；TRACE 全链
4. **NPU 修**：禁 `GlobalTensor::SetValue` 镜像 dk/c / 写 m；改 H2D 直读 + DataCopy（见 FEEDBACK）
5. 未抄禁树；未改 KB/DAG；Subagent 未碰 SSH/NPU

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | PASS_SYNC + PASS_IO（修后 kernel≈0.99s） |
| NPU 首轮 | FAIL `m@0` mismatch（exit 2，未挂） |
| SIM | **skip** |
| golden | `liboqs_pke_ref decrypt` |

## 硬锁

- `blockDim=1`
- CrossCore flag **仅 1/3**（L2/L3 各 launch 独立复用）；永禁 **5/7**
- Host 禁预喂最终 `m`
- out magic `0x543F0019`（T25）

运营回报：[`../../encrypt-rebuild-ops/tasks/T25-decrypt-device/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T25-decrypt-device/FEEDBACK.md)
