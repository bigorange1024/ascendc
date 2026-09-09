```
ID: PASS_CPU
cmd: cd graph-tests/dec_related/RB-D06-decaps-fo && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~5（编+跑≈19s；kernel≈1.05s×2 路径）
sync_audit: clean（无红线；仅 SYNC-09 PipeBarrier 粒度提示）
notes:
- 新建 `RB-D06-decaps-fo/`；basename=`decaps_fo_custom.cpp`；AIV-only、BLOCK_DIM=1、零 CrossCore。
- 设备 FO：c≟c'（标量逐字节）→ 选 K' 或 J(z‖c)=SHAKE256；K 经 UB+DataCopy 写出（X12）。
- 同 binary 两路径：FO_PATH=legit|reject；合法 c'=c；拒绝 c[-1]^=1 且 c'=合法原 c。
- 权威：liboqs_kem_ref KeyGen/Encaps/Decaps；K_legit/K_reject max=0；K_rej≠K_legit。
- 假绿三问：①golden=liboqs 非设备同源/非 python J；②权威已跑 cross_backend=liboqs；③单 AIV 无跨核 Sync 问题。
- 未跑 SIM/NPU（本刀门禁仅 CPU；禁 SSH/NPU）；未改 Decrypt KB/DAG。
next_hint: 主控可并行 -r npu；绿后派 K04
```

## 主控批注（2026-09-09）

- **采纳 PASS_CPU**：合法+拒绝；`K≡liboqs`；`K_rej≠K_legit`。
- **NPU**（910B3）：同双路径 PASS；legit `wall_sec=3.254`；reject `2.459`。

| 结论 | **DG6 关**；**Q-DECAPS-CORRECT 关**；开 **DRW-K04** |

ID: PASS_NPU  
exit: 0

## 假绿三问（书面）

1. **golden 是否与实现同源？** — 否。golden_K ← `liboqs_kem_ref` Decaps；设备仅 FO 选路；`hashlib.shake_256` 只诊断。
2. **权威交叉是否已跑？** — 是。`cross_backend.txt=liboqs`；缺库会 BLOCKED。
3. **跨核写读是否有 Sync？** — N/A（AIV-only，无 CrossCore）；K 整块对拍绿，X12 DataCopy。

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| 用例 STATUS | [`../../../dec_related/RB-D06-decaps-fo/STATUS.md`](../../../dec_related/RB-D06-decaps-fo/STATUS.md) |

## 主控补注（2026-09-09 16:49）

- **NPU×30 lean**：ok=30 fail=0；`ALL_DONE_K03_X30` @ 16:44:34（合法+拒绝双路径；`out/lib` LD_PATH）。
- 主控迟收口 ~5min（违规）；已立刻开 K01×30。

