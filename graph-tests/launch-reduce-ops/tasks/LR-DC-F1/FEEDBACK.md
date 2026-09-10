# LR-DC-F1 FEEDBACK

## RB-D08 单轮 NPU（2026-09-10）
```
ID: PASS_NPU
cmd: ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3
exit: 0
notes:
- Decrypt Host 3→2：prep → 融合 MIX(NTT+INTT)
- 根因修复：融合 ws 段2 偏移须 32B 对齐（d02 wssize=17976≡24 mod32），否则 w_hat 前 2 系数被 DataCopy 写坏
- m max_abs=0 vs liboqs_pke_ref；PASS_SYNC+PASS_IO
log: /mnt/workspace/launch-reduce-logs/d08-npu-alignfix.log
next: NPU×30 进行中
```

## RB-D08 NPU×30（2026-09-10）
```
ID: PASS_NPU_x30
summary: ok=30 fail=0
log: /mnt/workspace/launch-reduce-logs/d08-npu-x30-20260910-104504.log
```
