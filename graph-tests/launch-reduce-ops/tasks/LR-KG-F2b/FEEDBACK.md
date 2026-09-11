# LR-KG-F2b FEEDBACK

## RB-K08 单轮 NPU（2026-09-10）
```
ID: PASS_NPU
cmd: ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3
exit: 0
notes: PASS_SYNC + PASS_IO；ek/dk_kem max_abs=0 vs liboqs_kem_ref；Host 3-launch
log: /mnt/workspace/launch-reduce-logs/k08-npu-20260910-101409.log
next: NPU×30 进行中
```

## RB-K08 NPU×30（2026-09-10）
```
ID: PASS_NPU_x30
cmd: ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3 ×30
exit: 0
summary: ok=30 fail=0
log: /mnt/workspace/launch-reduce-logs/k08-npu-x30-20260910-101504.log
notes: KEM KeyGen Host 4→3 真机关闸
```
