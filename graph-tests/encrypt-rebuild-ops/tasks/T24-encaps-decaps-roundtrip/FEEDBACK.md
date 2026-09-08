# FEEDBACK — T24-encaps-decaps-roundtrip

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T24-encaps-decaps-roundtrip && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（编+跑≈24s；kernel≈4.84s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T11–T23) + SYNC-09 性能
sim: skip（战役默认 NPU 优先；长 SIM 非门禁；主控立刻推板）
notes:
- RB-T24：设备 Encaps(T23 路径)→(c,K)；liboqs Decaps(sk,c)→K'≡K；BLOCK_DIM=1；magic=0x543F0018。
- sk/ek←liboqs KeyGen；cross_backend=liboqs；缺库会 BLOCKED；Encaps golden 仅诊断。
- CPU：PASS_SYNC+PASS_RT+PASS_IO；K'≡K_dev≡G(m||H(ek))[:32]；中间量全 max=0；kernel≈4.84s。
- 未改 KB/DAG；未抄 encrypt/encaps/decaps/alg14/alg21/frozen；未碰 SSH/NPU；SIM skip。
next_hint: 主控立刻 -r npu；本机 SIM 非阻塞。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（CPU：双 launch + G + MU + BD12 + CBD + AHAT/YHAT + Cube 非零） |
| **PASS_RT** | 是（CPU：liboqs Decaps(sk, device_c)→K' ≡ K_dev） |
| **PASS_IO** | 是（CPU：h/coins/μ/u/v/t̂/Â/ŷ/yee/ρ + Encaps 诊断 c/K） |
| **SIM** | skip / pending（NPU 优先） |

## 挂点假设（上板）

| 现象 | 假设 |
|------|------|
| gen_data BLOCKED / cross_backend≠liboqs | 真机缺 liboqs_kem_ref；先 clone+build 或 rsync 静态 ref |
| PASS_RT 红但 c≡Encaps golden | Decaps 路径/sk 错位；查 sk.bin 与 KeyGen 同对 |
| K'≠K 且 c≠Encaps | 设备 Encaps 链；假绿三问：权威已跑？ |
| 无 PREP_DONE / 无 G_DONE | 卡 Launch1 / Sha3OneShot(H/G) |
| 有 CBD 无 AHAT / a_hat 半边 | 再查 BLOCK_DIM=1 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) | CPU 全绿摘要 |

## 实现 STATUS

[`../../../enc_related/RB-T24-encaps-decaps-roundtrip/STATUS.md`](../../../enc_related/RB-T24-encaps-decaps-roundtrip/STATUS.md)

## 主控批注

- **CPU**：回收 [T24 Encaps→Decaps往返](f5eedb3b-49d7-4f90-a1df-c904e0170688) PASS_CPU+RT（K'≡K_dev≡liboqs Decaps）。
- **NPU**：**T24_EXIT:0** + `[SUCCESS] PASS_SYNC+PASS_RT+PASS_IO`（与 T23 同批补库后）→ **双绿入账**；Encaps 权威往返闭合。
- **卡死压测（用户要求）**：910B3 连续 **`RB-T24` ×30**，`ok=30 fail=0`；无 BLOCKED / timeout124 / kernel 非 0。日志：`/mnt/workspace/encrypt-rebuild-hang-stress.log`（`HANG_STRESS_SUMMARY ok=30 fail=0`）。
