# FEEDBACK — T20-encaps-shaped

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T20-encaps-shaped && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（编+跑≈22s；kernel≈4.65s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T11/T15/T17/T19) + SYNC-09 性能
sim: skip（战役默认 NPU 优先；长 SIM 非门禁；主控立刻推板）
notes:
- RB-T20：Encaps 外形 + T19 Encrypt 全链；BLOCK_DIM=1。
- Host：μ←m（T04）；coins=r←G(m‖H(ek))；K_host 对拍；设备 Encrypt→c。
- 设备链同 T19：Decode₁₂→CBD→Â/ŷ→Mul/INTT→pack；Flag 1/3+4；永禁 5/7。
- CPU：PASS_SYNC+PASS_IO；c/K/μ/u/v/t̂/Â/ŷ/yee/ρ max=0；out=0x543F0014；kernel≈4.65s。
- TRACE：PREP→SET1/WAIT3→BD12→CBD→AHAT→YHAT→MUL→GATE→INTT→UV→PACK。
- 未改 KB/DAG；未抄 encrypt/encaps/alg14/alg20/frozen；未碰 SSH/NPU；SIM skip。
next_hint: 主控立刻 -r npu；设备侧 H/G→K 可后刀；本机 SIM 非阻塞。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（CPU：双 launch + BD12 + CBD + AHAT/YHAT + Cube 非零） |
| **PASS_IO** | 是（CPU：c + Host K + μ/u/v/t̂/Â/ŷ/yee/ρ 全对拍） |
| **SIM** | skip / pending（NPU 优先） |

## 挂点假设（上板）

| 现象 | 假设 |
|------|------|
| 无 PREP_DONE | 卡 Launch1 / coins=r 或 OFF_EK |
| 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 WAIT3 无 BD12 | 卡 Decode₁₂ |
| 有 BD12 无 CBD | 卡 PRF/CBD（coins 派生错则全程偏） |
| 有 CBD 无 AHAT / a_hat 半边 | 再查 BLOCK_DIM=1 |
| 有 AHAT 无 YHAT | 卡 ForwardNTT(y) |
| 有 YHAT 无 MUL/UV | 卡拓扑 Mul/INTT（μ/t̂ 错则 v/c 偏） |
| 有 UV 无 PACK | 卡 pack |
| c max≠0 / K mismatch | Encaps 头 H/G 或 μ←m 契约 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU 全绿 |

## 实现 STATUS

[`../../../enc_related/RB-T20-encaps-shaped/STATUS.md`](../../../enc_related/RB-T20-encaps-shaped/STATUS.md)

## 主控批注

- **CPU**：回收 [T20 Encaps外形](0f1e7f63-ac66-45fa-92cc-143d9fbe8bb4) PASS_CPU（c/K/μ max=0）。
- **NPU**：**T20_EXIT:0** → **双绿入账**；T21 上板 / T22 编码并行。
