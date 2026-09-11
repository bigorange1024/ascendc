# LR-W0-BASE FEEDBACK（片段）

## K04 PKE 3-launch 基线
```
ID: PASS
cmd: cd RB-K04-pke-full && ASCEND_DEVICE_ID=0 KERNEL_COMPUTE_BUDGET_SEC=180 bash run.sh -r npu -v Ascend910B3
exit: 0
wall_min: ~0.16 (kernel wall_sec≈9.8)
sync_audit: n/a (基线未改码)
notes:
- PASS_SYNC + PASS_IO ek/dk max=0 vs liboqs_pke_ref
- 三 launch TRACE GATE+handshake 可见
next_hint: K06 基线 → KG-F1 (RB-K07)
```
log: `/mnt/workspace/launch-reduce-logs/w0-k04.log`

## K06 KEM 4-launch 基线
```
ID: PASS
cmd: RB-K06-kem-full run.sh -r npu -v Ascend910B3
exit: 0
wall_min: ~kernel 2.2s
notes: PASS_SYNC+PASS_IO ek/dk_kem max=0 vs liboqs_kem_ref；四 launch
next_hint: D04/T26 基线；并行落地 RB-K07
```
log: `/mnt/workspace/launch-reduce-logs/w0-k06.log`

## D04 decrypt-full 基线（2026-09-10 重连后）
```
ID: PASS
cmd: ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3
exit: 0
notes: PASS_SYNC + PASS_IO m max_abs=0 vs liboqs_pke_ref；三 launch
log: /mnt/workspace/launch-reduce-logs/d04-npu-20260910-095618.log
```

## T26 decaps-device 基线（2026-09-10）
```
ID: FAIL_IO
cmd: ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3
exit: 1
notes: 走了拒绝路径 c'!=c → K=J(z||c)；K 与 golden 全不一致（mism~=32）。同步/TRACE 可见但 reenc 结果不等。
log: /mnt/workspace/launch-reduce-logs/t26-npu-20260910-095707.log
```

## K06 kem-full 基线（2026-09-10）
```
ID: PASS
cmd: ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3
exit: 0
notes: PASS_SYNC+PASS_IO ek/dk_kem max_abs=0 vs liboqs_kem_ref；四 launch
log: /mnt/workspace/launch-reduce-logs/k06-npu-20260910-095802.log
next: ×30 进行中；随后上 RB-K08（3 launch）
```

## K06 kem-full NPU×30（2026-09-10）
```
ID: PASS_NPU_x30
cmd: ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3 ×30
exit: 0
summary: ok=30 fail=0
log: /mnt/workspace/launch-reduce-logs/k06-npu-x30-20260910-095925.log
```
