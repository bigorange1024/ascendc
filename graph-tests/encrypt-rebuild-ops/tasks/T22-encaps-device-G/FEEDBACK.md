# FEEDBACK — T22-encaps-device-G

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T22-encaps-device-G && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（编+跑≈21s；kernel≈4.51s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T11/T15/T17/T19/T20/T21) + SYNC-09 性能
sim: skip（战役默认 NPU 优先；长 SIM 非门禁；主控立刻推板）
notes:
- RB-T22：T21 + 设备 (K‖r)←G(m‖H(ek))（shared Sha3OneShot）；BLOCK_DIM=1。
- Host：仅 m+ek；禁预喂 coins/K；prep 写 OFF_H/OFF_K/OFF_COINS；CBD 吃设备 r。
- CPU：PASS_SYNC+PASS_IO；c/K_dev/h/coins/μ/u/v/t̂/Â/ŷ/yee/ρ max=0；out=0x543F0016；kernel≈4.51s。
- TRACE：PREP→G_DONE→SET1/WAIT3→MU→BD12→CBD→AHAT→YHAT→MUL→GATE→INTT→UV→PACK。
- 未改 KB/DAG；未抄 encrypt/encaps/alg14/alg20/frozen；未碰 SSH/NPU；SIM skip。
next_hint: 主控立刻 -r npu；本机 SIM 非阻塞。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（CPU：双 launch + G + MU + BD12 + CBD + AHAT/YHAT + Cube 非零） |
| **PASS_IO** | 是（CPU：c + K_dev + h/coins + μ_dev/u/v/t̂/Â/ŷ/yee/ρ 全对拍） |
| **SIM** | skip / pending（NPU 优先） |

## 挂点假设（上板）

| 现象 | 假设 |
|------|------|
| 无 PREP_DONE / 无 G_DONE | 卡 Launch1 / Sha3OneShot(H/G) 或 OFF_M/EK |
| G_DONE 但 K/c 错 | H/G 输入拼接或字节序；对照 golden_h/K/coins |
| 有 G 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 WAIT3 无 MU | 卡 Decompress₁ / OFF_M |
| 有 MU 无 BD12 | 卡 Decode₁₂ |
| 有 BD12 无 CBD | 卡设备 coins→PRF/CBD |
| 有 CBD 无 AHAT / a_hat 半边 | 再查 BLOCK_DIM=1 |
| 有 AHAT 无 YHAT | 卡 ForwardNTT(y) |
| 有 YHAT 无 MUL/UV | 卡拓扑 Mul/INTT |
| 有 UV 无 PACK | 卡 pack |
| c max≠0 / K mismatch | 设备 G 或 Encaps 链 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) | CPU 全绿 |

## 实现 STATUS

[`../../../enc_related/RB-T22-encaps-device-G/STATUS.md`](../../../enc_related/RB-T22-encaps-device-G/STATUS.md)

## 主控批注

- **CPU**：回收 [T22设备G Encaps](2bc2bbc1-8058-4cd6-9820-6e87652167a4) PASS_CPU（c/K 对拍）。
- **NPU**：**T22_PRIORITY_EXIT:0** → **双绿入账**；T23 上板 / T24 往返并行。
