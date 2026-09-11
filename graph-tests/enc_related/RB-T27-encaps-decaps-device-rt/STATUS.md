# STATUS — RB-T27-encaps-decaps-device-rt

| 字段 | 值 |
|------|-----|
| 刀 | T27 · D-EXP-T27 |
| 状态 | **PASS_CPU + PASS_NPU**（auto_gen 撞名修复后）；SIM **skip** |
| 日期 | 2026-09-08 |
| 墙钟 | CPU 全链路≈31s；kernel≈9.9s |

## 目标达成

1. 设备 Encaps→Decaps：`K_decaps ≡ K_encaps ≡ liboqs`；`c ≡ golden_c`
2. 七 launch；`BLOCK_DIM=1`；旗 1/3(+4)；永禁 5/7
3. **X12 全覆盖**：reenc h/K/coins/ek；decrypt prep 直读 + DataCopy ŝ/u/v；intt m' DataCopy；pack c DataCopy
4. 未抄禁树；未改 KB/DAG；未碰 SSH/NPU

## NPU 首跑 FAIL → 根因

| 现象 | 根因 |
|------|------|
| 未卡死；`K_decaps != K_encaps mism~=32` | 从 T26 复制的 decrypt 仍 `GlobalTensor::SetValue` 镜像 dk/c、写 m'（CPU 假绿） |

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4`（X12 修后） | PASS_SYNC + PASS_IO + PASS_CROSS（kernel≈9.9s） |
| SIM | **skip** |
| NPU（撞名修复后） | **PASS**：`dec_prep`+`enc_prep` 均在 `device_aiv.o`；`c'==c`；`wall≈3.2s` |

## 硬锁

- `blockDim=1`；Decrypt 1/3；Encaps/Reenc 1/3+4；永禁 **5/7**
- Host 禁预喂 `c` / 最终 `K` / `m'`
- out magic `0x543F001B`

运营回报：[`../../encrypt-rebuild-ops/tasks/T27-encaps-decaps-device-rt/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T27-encaps-decaps-device-rt/FEEDBACK.md)
