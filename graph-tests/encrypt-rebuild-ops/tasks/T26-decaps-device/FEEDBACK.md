# FEEDBACK — T26-decaps-device

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T26-decaps-device && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（编+跑≈25s；kernel≈6.5s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T12/T22/T25) + SYNC-09 性能
sim: skip（战役默认 NPU 优先；长 SIM 非门禁；主控立刻推板）
notes:
- RB-T26：Alg.21 Decaps 五 launch（Decrypt L1–3 + Reenc L4–5）；BLOCK_DIM=1。
- Flag：Decrypt 仅 1/3；Reenc 1/3+4=GATE；未用 SoftSync / flag 5·7。
- CPU：PASS_SYNC+PASS_IO；K≡liboqs Decaps；c'==c；out=0x543F001A；kernel≈6.5s。
- 编排自 RB-T25 Decrypt + RB-T23 Encaps 树（本目录 decrypt/ + reenc/）；禁抄 alg21。
- 未改 KB/DAG；未碰 SSH/NPU；SIM skip。
next_hint: 主控立刻 -r npu；本机 SIM 非阻塞；下一刀 T27 往返压测。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（CPU：Decrypt+Reenc TRACE 全链） |
| **PASS_IO** | 是（CPU：K ≡ liboqs Decaps） |
| **SIM** | skip / pending（NPU 优先） |

## 挂点假设（上板）

| 现象 | 假设 |
|------|------|
| 无 DEC PREP | 卡 Launch1 / ByteDecode / Decompress |
| 有 DEC 无 ENC PREP/G | 卡 mid 后 m' 传递或 enc_prep |
| 有 G 无 PACK | 卡 Reenc MIX / GATE(4) |
| K max≠0 且 c'≠c | Decrypt 或重加密错；查 m'/coins |
| K 错但 c'==c | G 头或 Host 选路 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU 尾摘 |

## 实现 STATUS

[`../../../enc_related/RB-T26-decaps-device/STATUS.md`](../../../enc_related/RB-T26-decaps-device/STATUS.md)

## 主控批注

- **CPU**：回收 [T26 设备Decaps重建](b4c438f7-3a4c-4e9a-aec9-3ae12e8b5224) PASS（K≡liboqs；c'==c；五 launch）。
- **NPU**：首轮因缺远程 `tiny_sha3` CMake 失败（EXIT:1）；已补源。待 T27 修通后一并复测（decrypt 树仍有 X12 `SetValue` 写 GM 风险，与 T27 同修）。
