# FEEDBACK — T19-encrypt-ek-decode-full

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T19-encrypt-ek-decode-full && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（编+跑≈21s；kernel≈4.7s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T11/T15/T17) + SYNC-09 性能
sim: skip（战役默认 NPU 优先；长 SIM 非门禁；主控立刻推板）
notes:
- RB-T19：单库双 launch；Launch1 prep(coins+完整 ek→OFF_EK+ρ)；Launch2 MIX：Decode₁₂→CBD→Â/ŷ→Mul/INTT→pack→c。
- Host 禁预喂 t̂；ek 体=BE₁₂(t̂)；设备 poly_byte_decode12_scalar_gm→OFF_T_HAT（T18 契约）。
- 硬锁 F203_AHAT16_BLOCK_DIM=1（#undef+#define + npu_lib ascendc_compile_definitions）。
- Flag：1/3 复用 + 4=GATE；永禁 5/7。
- CPU：PASS_SYNC+PASS_IO；c/t̂/u/v/Â/ŷ/yee/ρ max=0；out=0x543F0013；kernel≈4.65s。
- TRACE：PREP→SET1/WAIT3→BD12→CBD→AHAT→YHAT→MUL→GATE→INTT→UV→PACK。
- 未改 KB/DAG；未抄 encrypt/alg14/frozen；未碰 SSH/NPU；SIM skip。
next_hint: 主控立刻 -r npu；关注 a_hat 全 16 poly + t̂ Decode；本机 SIM 非阻塞。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（CPU：双 launch + BD12 + CBD + AHAT/YHAT + Cube 非零） |
| **PASS_IO** | 是（CPU：c/t̂/u/v/Â/ŷ/yee/ρ 全对拍 max=0） |
| **SIM** | skip / pending（NPU 优先） |

## 挂点假设（上板）

| 现象 | 假设 |
|------|------|
| 无 PREP_DONE | 卡 Launch1 / OFF_EK 镜像 |
| 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 WAIT3 无 BD12 | 卡 Decode₁₂ / OFF_EK 布局 |
| 有 BD12 无 CBD | 卡 PRF/CBD |
| 有 CBD 无 AHAT / a_hat 半边 | 再查 BLOCK_DIM=1 |
| 有 AHAT 无 YHAT | 卡 ForwardNTT(y) |
| 有 YHAT 无 MUL/UV | 卡拓扑 Mul/INTT（t̂ 错则 v/c 偏） |
| 有 UV 无 PACK | 卡 pack |
| t̂/c max≠0 | ek BE₁₂ 布局或 Decode 契约 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) / [`logs/cpu-excerpt.txt`](logs/cpu-excerpt.txt) | CPU 全绿 |

## 实现 STATUS

[`../../../enc_related/RB-T19-encrypt-ek-decode-full/STATUS.md`](../../../enc_related/RB-T19-encrypt-ek-decode-full/STATUS.md)

## 主控批注

- **CPU**：回收 [T19全链含Decode12](9f00e978-7b2b-49cd-b7ee-55ffc474a18e) PASS_CPU（c/t̂/u/v/Â/ŷ max=0）。
- **NPU**：**T19_EXIT:0** → **双绿入账**；并行 T20 上板 / T21 编码。
