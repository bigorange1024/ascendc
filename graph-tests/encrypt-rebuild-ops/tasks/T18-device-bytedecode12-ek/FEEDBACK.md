# FEEDBACK — T18-device-bytedecode12-ek

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T18-device-bytedecode12-ek && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（kernel≈0.5s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装假阳性，同 T02/T16) + SYNC-09 性能
sim: skip（NPU 优先；长 SIM 非门禁）
notes:
- 新建 RB-T18：单 launch MIX；Host ek[1568]=BE₁₂‖ρ；AIV0 ByteDecode₁₂→t̂[1024]。
- Flag：仅 1/3；永禁 5/7；无 GATE；AIC 极轻 Cube×1。
- 积木：shared poly_byte_decode12_scalar_gm；契约对齐 T05，未大段抄 T05/encrypt/alg14/frozen。
- CPU PASS_SYNC+PASS_IO（t_hat max_abs=0）；未改 KB/DAG；未碰 NPU/SSH；SIM skip。
next_hint: 主控立刻 rsync `-r npu`。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_CPU** | 是（编通 + PASS_SYNC + PASS_IO） |
| **PASS_SYNC** | 是（BD12 1/3 因果 + Cube 非零 + sync 无红线） |
| **PASS_IO** | 是（t̂ 与 golden max_abs=0） |
| **SIM** | skip |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡 Decode 握手 |
| 有 POST_WAIT3 无 BD12_DONE | 卡 Decode / 写出 |
| t̂ 有但 max≠0 | ek 体布局 / Decode₁₂ 契约差 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU 绿 |

## 实现 STATUS

[`../../../enc_related/RB-T18-device-bytedecode12-ek/STATUS.md`](../../../enc_related/RB-T18-device-bytedecode12-ek/STATUS.md)

## 主控批注

- **CPU**：回收 [T18设备Decode12](506bdb23-b6c6-4119-adc9-2ca5d21c7aa5) PASS_CPU（t̂ max=0）。
- **NPU**：**T18_EXIT:0** → **双绿入账**。
