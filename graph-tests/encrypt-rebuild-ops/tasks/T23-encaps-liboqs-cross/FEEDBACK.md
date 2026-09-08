# FEEDBACK — T23-encaps-liboqs-cross

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS_CPU
cmd: cd graph-tests/enc_related/RB-T23-encaps-liboqs-cross && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~1（编+跑≈22s；kernel≈4.62s）
sync_audit: 无红线；SYNC-05(高: Wait→CrossSet 薄封装误解析假阳性，同 T11–T22) + SYNC-09 性能
sim: skip（战役默认 NPU 优先；长 SIM 非门禁；主控立刻推板）
notes:
- RB-T23：T22 设备 Encaps + liboqs ML-KEM-1024 交叉；BLOCK_DIM=1；magic=0x543F0017。
- ek←liboqs KeyGen；golden_c/K←liboqs Encaps（同固定 m）；cross_backend=liboqs；缺库会 BLOCKED。
- CPU：PASS_SYNC+PASS_CROSS+PASS_IO；设备 c/K ≡ liboqs；中间量全 max=0；kernel≈4.62s。
- Host 诊断 Encrypt c 在 gen_data 已与 liboqs 对齐（防自洽假绿）。
- 未改 KB/DAG；未抄 encrypt/encaps/alg14/alg20/frozen；未碰 SSH/NPU；SIM skip。
next_hint: 主控立刻 -r npu；本机 SIM 非阻塞。
```

## 分栏

| 栏 | 结果 |
|----|------|
| **PASS_SYNC** | 是（CPU：双 launch + G + MU + BD12 + CBD + AHAT/YHAT + Cube 非零） |
| **PASS_CROSS** | 是（CPU：c/K_dev ↔ liboqs Encaps 同 m/ek 逐字节） |
| **PASS_IO** | 是（CPU：h/coins/μ/u/v/t̂/Â/ŷ/yee/ρ 全对拍） |
| **SIM** | skip / pending（NPU 优先） |

## 挂点假设（上板）

| 现象 | 假设 |
|------|------|
| gen_data BLOCKED / cross_backend≠liboqs | 真机缺 liboqs_kem_ref；先 clone+build 或 rsync 静态 ref |
| 无 PREP_DONE / 无 G_DONE | 卡 Launch1 / Sha3OneShot(H/G) 或 OFF_M/EK |
| G_DONE 但 K≠liboqs | H/G 输入拼接；对照 golden_K |
| c≠liboqs / K ok | Encrypt 链或 pack；假绿三问：权威已跑？ |
| 有 G 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 CBD 无 AHAT / a_hat 半边 | 再查 BLOCK_DIM=1 |

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| [`logs/cpu.log`](logs/cpu.log) | CPU 全绿摘要 |

## 实现 STATUS

[`../../../enc_related/RB-T23-encaps-liboqs-cross/STATUS.md`](../../../enc_related/RB-T23-encaps-liboqs-cross/STATUS.md)

## 主控批注

- **CPU**：回收 [T23 Encaps×liboqs交叉](1edad27a-a413-4a59-b736-14b24c2ed385) PASS_CPU+CROSS（c/K≡liboqs）。
- **NPU**：首轮 BLOCKED（缺 aarch64 liboqs）→ 远程 `build-liboqs`+`liboqs_kem_ref` 后 **T23_EXIT:0** + `[SUCCESS] PASS_SYNC+PASS_CROSS+PASS_IO` → **双绿入账**。
