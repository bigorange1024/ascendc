# Agent 交接 — 每日刷新

> **最后刷新**：2026-09-11（新环境：加法已绿；TS 节点在线但 serve:2222 断；等跳板修壳）  
> Git：未授权不 commit/push

## 白话现状

- **新机已重建**：物理卡是 `/dev/davinci2`（不是旧的 davinci7），加法 NPU 对拍**已绿** → 卡干净。
- **完整仓已传到** `/workspace/ascendc`（含贯通链 EN12、hidevlab 脚本）。
- **Tailscale 节点仍在**：`hidevlab-npu` / `100.70.113.117`，ping 通。
- **但 Agent SSH 断了**：userspace 入站靠 `serve tcp:2222`；现在 SOCKS 连 2222 失败（多半是机上 `sshd:2222` 或 `serve` 掉了）。**不是**让你再在 WebIDE 跑整段入网。

## 硬约束

- 只做**新贯通链**（`graph-tests/enc_cann_ntt/`）；**禁止**再跑旧 Encaps 猎挂。
- **禁止**拉看门狗/心跳保活。

## P0（请人选一条）

1. **推荐**：控制台再点一次 **SSH直连**，把 `ssh -J …` + 密码贴过来 → 我远程只重启 `sshd:2222` + `tailscale serve`，然后继续 EN12 CPU→NPU。  
2. 或者在 WebIDE **只跑下面短修复**（不要跑整段 `hidevlab_ts.sh`）：

```bash
TS=/workspace/tailscale/bin/tailscale; SOCK=/workspace/tailscale/run/tailscaled.sock
pkill -f 'sshd.*2222' 2>/dev/null || true
/usr/sbin/sshd -p 2222 -o ListenAddress=127.0.0.1 -o PermitRootLogin=prohibit-password -o PasswordAuthentication=no -o PubkeyAuthentication=yes -o PidFile=/tmp/sshd2222.pid
"$TS" --socket="$SOCK" serve --bg --tcp 2222 tcp://127.0.0.1:2222
"$TS" --socket="$SOCK" serve status | head
```

修好后说一声，我继续跑「SampleNTT 贯通链粘性多轮」真机实验。
