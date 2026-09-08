# FEEDBACK — N00

```
ID: N00
cmd: flock + ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3 (add_custom; clean rebuild)
exit: 0
wall_min: ~1 (SSH+worktree ~10s; first dirty run fail ~11s; clean SUCCESS ~11s; total ≪20)
npu: ok
ts_ip: 100.97.98.72 (MagicDNS cannlab-npu-1; sshd 2222; SOCKS 127.0.0.1:1055)
worktree: ready (/mnt/workspace/ascendc-encrypt-rebuild from git worktree add HEAD @6db3f62)
notes: |
  - userspace tailscale + ~/.ssh/cannlab 已通；物理节点 /dev/davinci7，ACL 逻辑 ASCEND_DEVICE_ID=0
  - 首跑未清 build/output：kernel 跑完但对拍 FAIL（md5 异；自 float idx 2048 起偏）
  - 二次：rm -rf build output input + CANNLAB=1/NPU_SINGLE_CARD=1 + flock → [SUCCESS] output matches golden
  - 未跑 Encrypt/MIX；未 kill -TERM 1；未空等
next_hint: N01 可在同 worktree 上复跑已 SIM 证过的刀；脏树先清 build/output 再上板
```

## 验收对照

| # | 结果 |
|---|------|
| A1 | ssh 通（developer@100.97.98.72:2222 via SOCKS:1055） |
| A2 | add_custom NPU **SUCCESS**（干净重跑） |
| A3 | 本文件 + `logs/n00-add-custom-npu-20260908.log`；墙钟 ≪20 min |
| A4 | 未跑未证 Encrypt FSM |

## 关键步骤摘要

1. `tailscale status`：`cannlab-npu-1` = 100.97.98.72 idle  
2. `git worktree add /mnt/workspace/ascendc-encrypt-rebuild HEAD`（自 `/mnt/workspace/ascendc`）  
3. `flock /mnt/workspace/.npu.lock` + driver `LD_LIBRARY_PATH` + `source …/set_env.sh`  
4. `ASCEND_DEVICE_ID=0 CMAKE_BUILD_JOBS=8 bash run.sh -r npu -v Ascend910B3`

## 日志

- [`logs/n00-add-custom-npu-20260908.log`](logs/n00-add-custom-npu-20260908.log)
- 历史：[`logs/n00-connectivity-20260908-122803.log`](logs/n00-connectivity-20260908-122803.log)（首次 blocked_auth）

## 主控批注

- 2026-09-08 首次回收：`blocked_auth`（无密钥）。  
- 密钥就位后重派：**npu: ok**，add_custom SUCCESS。  
- 沉淀 X8：脏 `build/output` 可致对拍假红；上板前清产物。  
- **立刻接 N01**（T01 NPU），禁空闲。
