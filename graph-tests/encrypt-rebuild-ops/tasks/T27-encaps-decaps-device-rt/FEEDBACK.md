# FEEDBACK — T27-encaps-decaps-device-rt

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T27-encaps-decaps-device-rt && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（编+跑≈31s；kernel≈9.9s）
sync_audit: 无红线；SYNC-05 假阳性 + SYNC-09（同前；本轮未重扫必改）
sim: skip
notes:
- 根因(X12)：T26 复制的 decrypt 仍用 GlobalTensor::SetValue
  (1) prep 镜像 dk/c→ws；(2) intt 写 m'/mOut。
  NPU 标量写 GM 不可靠 → Decrypt/m' 错 → K_decaps≠K_encaps（mism≈32）；未挂死。
- 改动：prep 直读 H2D dkIn/cIn + DataCopy 写 ŝ/u/v；intt m 经 UB+DataCopy（对齐 T25 X12）。
  reenc h/K/coins/ek 前轮已 DataCopy；pack/c 既有 DataCopy。
- CPU 复验：PASS_SYNC+PASS_IO+PASS_CROSS；K'≡K≡liboqs；c'==c。
- 未改 KB/DAG；未碰 SSH/NPU；禁树未碰。
next_hint: 主控再推 -r npu；绿后再 ×N 压测关 Q-RT-HANG。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（CPU：Encaps+Decrypt+Reenc TRACE） |
| **PASS_IO** | 是（CPU：K_decaps ≡ K_encaps） |
| **PASS_CROSS** | 是（CPU：c/K ≡ liboqs） |
| **SIM** | skip |
| **NPU 上轮** | FAIL I/O（X12）；本轮待主控重推 |

## 根因 / 改动

| 项 | 说明 |
|----|------|
| 假绿三问 | golden=liboqs ✓；交叉有 ✓；非 Sync 挂 → X12 GM 写出 |
| 漏改 | `decrypt/prep_device_math.hpp` SetValue 镜像；`decrypt/intt_device_math.hpp` SetValue 写 m |
| 修复 | 直读输入 + UB/`DataCopy` 写出（同 T25 NPU 绿路径） |

## 挂点假设（再上板）

| 现象 | 假设 |
|------|------|
| 仍 K'≠K | 查 Encaps→Decaps 间 cEncDev 是否被覆盖；D2H m' |
| 挂死 | 非本刀主假设（上轮未挂） |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU 尾摘（X12 修后） |

## 实现 STATUS

[`../../../enc_related/RB-T27-encaps-decaps-device-rt/STATUS.md`](../../../enc_related/RB-T27-encaps-decaps-device-rt/STATUS.md)

## 主控批注

- **CPU**：回收 [T27设备EncapsDecaps往返](f70a9baf-1534-4b07-97c1-47f96f495a8a) PASS（K'≡K≡liboqs；七 launch）。
- **NPU**：补 `tiny_sha3` 后首跑 **未挂** 但 `K_decaps != K_encaps mism~=32`（`STRESS_1_EXIT:1`）。假绿三问：golden/交叉=liboqs；非挂→查跨段 GM / X12。回炉修。
- **回炉**：回收 [修T27 NPU K往返](f70a9baf-1534-4b07-97c1-47f96f495a8a) — decrypt X12（prep 直读 + m DataCopy）；CPU 复绿。
- **NPU 复测**（`cannlab-npu-1`）：曾 `507000` @ `dec_prep` / TRACE PREP=0。
- **根因**：`decrypt/prep_custom.cpp` 与 `reenc/prep_custom.cpp` **同 basename** → auto_gen 撞车，最终 `device_aiv.o` **只保留 enc_prep**，`dec_prep` 丢失 → launch 507000。
- **修复**：改名为 `dec_prep_custom.cpp` / `enc_prep_custom.cpp`（T26 同步）；重编后 **NPU PASS**（`PREP=0x50524550`，`c'==c → K=K'`，`wall≈3.2s`，`T27_NAMEFIX_EXIT:0`）。stream recreate 非根因（可留作防二次 AIV 保险）。
