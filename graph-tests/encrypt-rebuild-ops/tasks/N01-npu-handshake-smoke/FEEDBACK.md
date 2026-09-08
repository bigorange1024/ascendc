# FEEDBACK — N01

```
ID: PASS
cmd: 主控直跑 rsync + ASCEND_DEVICE_ID=0 flock run.sh -r npu（log n01-npu-20260908-044640.log）
exit: 0
wall_min: ~1（kernel≈2.66s）
npu: ok
notes: |
  - 主控权威结果：[SUCCESS] RB-T01-mix-ntt13-handshake (npu)；N01_EXIT:0
  - 另有子 Agent 竞态跑出 verify exit 1（AIV0 TRACE 硬条件）；已令其停远程
  - 结论：握手 NPU 不挂且主控路径绿；TRACE 槽 0 仍属 X7 soft，勿当 hang
next_hint: NPU 只主控；≤3min keepalive；下一刀等 T02 SIM PASS 后主控上板
```

## 日志

- 主控 PASS：[`logs/n01-npu-20260908-044640.log`](logs/n01-npu-20260908-044640.log)
- 子 Agent 竞态（参考）：`logs/n01-t01-npu-20260908-044548.log`（若存在）

## 主控批注

- 2026-09-08：NPU/SSH **禁止**再派 Subagent；空闲阈值 **4 min**；主控 ≤3 min keepalive。
- 子 Agent 已 `stopped: npu_handed_to_main`。
