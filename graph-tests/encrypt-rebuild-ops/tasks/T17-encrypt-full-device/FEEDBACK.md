# FEEDBACK — T17-encrypt-full-device

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T17-encrypt-full-device && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（编码收尾+修 BLOCK_DIM + CPU≈21s 含编；kernel≈4.4s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T11/T15) + SYNC-09 性能
sim: skip（战役默认 NPU 优先；长 SIM 非门禁；主控立刻再推修版上板）
notes:
- RB-T17：单库双 launch；Launch1 prep(coins+ρ)；Launch2 MIX：CBD→Â/ŷ→Mul/INTT→pack→c。
- 修 NPU FAIL_IO a_hat 半边：硬锁 F203_AHAT16_BLOCK_DIM=1（#undef+#define）+ npu_lib ascendc_compile_definitions（T13 同症）。
- Flag：1/3 复用 + 4=GATE；永禁 5/7；Host 仅 ek/coins/μ/t̂/ζ/γ；禁预喂最终 y/e/Â/ŷ/u/v/c。
- CPU：PASS_SYNC+PASS_IO；c match；u/v/Â/ŷ/yee max_abs=0；kernel≈4.4s；out=0x543F0011。
- TRACE：PREP→SET1/WAIT3→CBD→AHAT→YHAT→MUL→GATE→INTT→UV→PACK 全链。
- 未改 KB/DAG；未抄 encrypt/alg14/frozen；未碰 SSH/NPU；SIM skip。
next_hint: 主控立刻 -r npu 再推本修版；关注 a_hat 全 16 poly；本机 SIM 非阻塞。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（CPU：双 launch + CBD + AHAT/YHAT + Cube 非零） |
| **PASS_IO** | 是（CPU：c/u/v/Â/ŷ/yee/ρ 全对拍 max=0） |
| **SIM** | skip / pending（NPU 优先） |

## 挂点假设（上板）

| 现象 | 假设 |
|------|------|
| 无 PREP_DONE | 卡 Launch1 |
| 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 WAIT3 无 CBD | 卡 PRF/CBD |
| 有 CBD 无 AHAT / a_hat 半边 | 再查 BLOCK_DIM=1 / ascendc_compile_definitions |
| 有 AHAT 无 YHAT | 卡 ForwardNTT(y) |
| 有 YHAT 无 MUL/UV | 卡拓扑 Mul/INTT |
| 有 UV 无 PACK | 卡 pack |
| c/u/v max≠0 | CBD 噪或 Â/ŷ 契约 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) | CPU 全绿 |

## 实现 STATUS

[`../../../enc_related/RB-T17-encrypt-full-device/STATUS.md`](../../../enc_related/RB-T17-encrypt-full-device/STATUS.md)

## 主控批注

- **CPU**：回收 [T17全设备Encrypt](42f45221-c630-47b5-85a5-136a49cff151) PASS_CPU；`BLOCK_DIM=1` 修半边 Â。
- **NPU**：**T17_PRIORITY_EXIT:0** PASS_SYNC+PASS_IO（c/u/v/Â/ŷ）→ **双绿入账**。
