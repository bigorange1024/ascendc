# FEEDBACK — T15-encrypt-ahat-yhat-uv-c

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T15-encrypt-ahat-yhat-uv-c && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（编码后 CPU≈25s 含编；kernel≈6.6s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T11/T14) + SYNC-09 性能
sim: skip（战役默认 NPU 优先；长 SIM 非门禁；主控立刻上板）
notes:
- RB-T15：单库双 launch；Launch1 prep(AIV)；Launch2 MIX：设备 Â←SampleNTT(ρ)+ŷ←NTT(y)→Mul/INTT→pack→c。
- Flag：1/3 复用 + 4=GATE；永禁 5/7；AIC Cube×2。
- Host 仅预喂 t̂/e/μ/ζ/γ；禁预喂最终 Â/ŷ/u/v。
- CPU：PASS_SYNC+PASS_IO；c match；u/v/Â/ŷ max_abs=0；kernel≈6.6s。
- TRACE：PREP→SET1/WAIT3→AHAT→YHAT→MUL→GATE→INTT→UV→PACK 全链；out=0x543F003F。
- 未改 KB/DAG；未抄 encrypt/alg14/frozen；未碰 SSH/NPU；未并行第二路 SIM。
next_hint: 主控立刻 -r npu 推板；本机 SIM 非阻塞，有余量可后补。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（CPU：双 launch 因果 + AHAT/YHAT + Cube 非零） |
| **PASS_IO** | 是（CPU：c/u/v/Â/ŷ 全对拍 max=0） |
| **SIM** | skip / pending（NPU 优先） |

## 挂点假设（上板）

| 现象 | 假设 |
|------|------|
| 无 PREP_DONE | 卡 Launch1 |
| 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 WAIT3 无 AHAT | 卡 SampleNTT / BLOCK_DIM |
| 有 AHAT 无 YHAT | 卡 ForwardNTT |
| 有 YHAT 无 MUL/UV | 卡拓扑 Mul/INTT |
| 有 UV 无 PACK | 卡 pack |
| c/u/v max≠0 | Â/ŷ 契约或噪预喂接线 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU 全绿 |

## 实现 STATUS

[`../../../enc_related/RB-T15-encrypt-ahat-yhat-uv-c/STATUS.md`](../../../enc_related/RB-T15-encrypt-ahat-yhat-uv-c/STATUS.md)

## 主控批注

- **CPU**：回收 [T15 Encrypt半链拼装](e4067066-28e6-45a6-86e3-ee9b923add08) — PASS_CPU（c/u/v/Â/ŷ max=0）；SIM skip。
- **NPU**：**T15_EXIT:0** — PASS_SYNC+PASS_IO → **双绿入账**。远端 nohup 循环仍在跑 sticky（当前编 T11）防 IDLE。
