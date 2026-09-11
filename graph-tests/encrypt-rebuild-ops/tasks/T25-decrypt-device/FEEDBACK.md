# FEEDBACK — T25-decrypt-device

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T25-decrypt-device && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（修后编+跑≈17s；kernel≈0.99s）
sync_audit: 无红线；SYNC-05 假阳性 + SYNC-09（同前；本轮未重扫必改）
sim: skip
notes:
- NPU 红因假设（未卡死、m@0=0x41≠0x1b）：GlobalTensor::SetValue 镜像 dk/c 与写 m 在实机不可靠；
  CPU 孪生标量 GM 写可读 → 假绿。权威 golden=liboqs PKE Decrypt 仍正确。
- 改动：prep 直读 H2D 的 dkIn/cIn；c 经 DataCopy→UB 再 ByteDecode_d；m 经 UB+DataCopy 写出
  （对齐 T22 pack）。禁 SoftSync/5/7；拓扑不变。
- CPU 复验仍 PASS_SYNC+PASS_IO（m≡liboqs）。请主控再推 -r npu。
next_hint: 主控立刻 bash run.sh -r npu -v Ascend910B4；若仍红，抓 output/{s_hat,u,v,u_hat,w,m}.bin 回传分 launch 对拍。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（CPU 复验） |
| **PASS_IO** | 是（CPU：m ≡ liboqs） |
| **SIM** | skip |
| **NPU（上轮）** | FAIL m mismatch（exit 2，未挂）；本轮修复待主控复测 |

## 假绿三问（本轮）

1. golden=liboqs PKE Decrypt（非自实现同源）✓  
2. 权威已用 ✓  
3. 非 Sync 挂 → **GM 标量写路径**（SetValue 镜像/写 m）相对 DataCopy 的 NPU 差异 ✓

## 挂点假设（再上板）

| 现象 | 假设 |
|------|------|
| m 仍错、ŝ/u/v 对 | INTT/Compress 或 w_hat 跨 launch 可见性 |
| ŝ/u/v 错 | ByteDecode/Decompress 仍有搬运问题 |
| 再挂死 | 非本刀预期（上轮未挂） |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | 首轮 audit |
| [`logs/cpu.log`](logs/cpu.log) | 首轮 CPU |

## 实现 STATUS

[`../../../enc_related/RB-T25-decrypt-device/STATUS.md`](../../../enc_related/RB-T25-decrypt-device/STATUS.md)

## 主控批注

- **NPU 首轮**：`T25_PRIORITY_EXIT:2`，`m != golden @0`，Stream 返回未卡死。
- **修版复测**：`T25_FIX_PRIORITY_EXIT:0`，`m == golden_m`（liboqs）→ **双绿入账**。根因假设成立：`GlobalTensor::SetValue` 写 GM 在 NPU 假绿。
- 并行 [T26 设备Decaps重建](b4c438f7-3a4c-4e9a-aec9-3ae12e8b5224)。
