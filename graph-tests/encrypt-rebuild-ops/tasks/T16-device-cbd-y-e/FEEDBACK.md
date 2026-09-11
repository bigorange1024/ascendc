# FEEDBACK — T16-device-cbd-y-e

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T16-device-cbd-y-e && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（独占重跑；kernel≈0.6s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装假阳性，同 T02/T03/T12/T13) + SYNC-09 性能
sim: skip（NPU 优先；长 SIM 非门禁）
notes:
- 新建 RB-T16：单 launch MIX；Host coins（T07 FIXED_COINS）；AIV0 SHAKE256 PRF(N=0..8)+Alg.8 CBD η=2→y‖e1‖e2。
- Flag：仅 1/3；永禁 5/7；无 GATE；AIC 极轻 Cube×1。
- 积木：-I alg8 OneRowUb（F203_CBD_BLOCK_DIM=1）+ shake_xof；未抄 encrypt/alg14/frozen。
- CPU PASS_SYNC+PASS_IO（y/e1/e2 max=0）；主控先前 cpu-main.log 失败为并发 rm build 竞态（.o.d missing），非逻辑错。
- 未改 KB/DAG；未碰 NPU/SSH；SIM skip。
next_hint: 主控立刻 rsync `-r npu`；远端勿与本机同时 cmake 同一目录。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_CPU** | 是（编通 + PASS_SYNC + PASS_IO） |
| **PASS_SYNC** | 是（CBD 1/3 因果 + Cube 非零 + sync 无红线） |
| **PASS_IO** | 是（y/e1/e2 与 golden max_abs=0） |
| **SIM** | skip |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 POST_WAIT3 | 卡 CBD 握手 |
| 有 POST_WAIT3 无 CBD_DONE | 卡 PRF/CBD / 写出 |
| YEE 有但 max≠0 | coins / SHAKE256 / CBD 契约差 |
| cmake `.o.d` missing | 与另一路 `rm -rf build` 竞态 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | 独占 CPU 绿 |
| [`logs/cpu-main.log`](logs/cpu-main.log) | 主控竞态失败记录 |

## 实现 STATUS

[`../../../enc_related/RB-T16-device-cbd-y-e/STATUS.md`](../../../enc_related/RB-T16-device-cbd-y-e/STATUS.md)

## 主控批注

- **CPU**：回收 [T16设备CBD](a407aec5-43b7-40e3-bea2-7379420dab79) PASS_CPU；主控竞态失败作废。
- **NPU**：**T16_PRIORITY_EXIT:0** PASS_SYNC+PASS_IO（y/e1/e2）→ **双绿入账**。
