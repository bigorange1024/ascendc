# Agent 交接 — 每日刷新

> **最后刷新**：2026-09-11（fe53 并入 ffc6 WebIDE 连接链路；加法已绿；TS 节点在线但 serve:2222 曾断）  
> 基准分支：`cursor/cann-ntt-operator-refactor-fe53`  
> Git：未授权不 commit/push

## 连接入口（从 ffc6 合入）

| 环境 | 主路径 | 文档 |
|------|--------|------|
| **HiDevLab**（当前） | 人：WebIDE 跑 `webide_start.sh`；Agent：Cursor 上 `agent_connect.sh` | **[`docs/engineering/HiDevLab-Agent连接.md`](docs/engineering/HiDevLab-Agent连接.md)** · [`HiDevLab-WebIDE操作手册.md`](docs/engineering/HiDevLab-WebIDE操作手册.md) |
| GitCode CANNLab | Tailscale + `scripts/cannlab/` | [`docs/engineering/CANNLab接入与远程驱动.md`](docs/engineering/CANNLab接入与远程驱动.md) |

HiDevLab **WebIDE 常不能 git** → 人侧脚本放 `/workspace/user_data/`；**Agent 在 Cursor 机器上基于 `fe53` git pull** 拿连接脚本。  
SSH直连约 5 分钟票 → **非** Agent 主通道。

```bash
# Cursor Agent（本仓 fe53）
git fetch origin cursor/cann-ntt-operator-refactor-fe53 && git checkout cursor/cann-ntt-operator-refactor-fe53
bash scripts/hidevlab/agent_connect.sh
bash scripts/hidevlab/agent_connect.sh --exec 'hostname; npu-smi info | head'
```

Secret：`TS_AUTHKEY` / `TAILSCALE_AUTHKEY` + `CANNLAB_SSH_KEY`（坏格式可用，`materialize_ssh_key.sh` 会重建）。

人侧每次开机（WebIDE）：

```bash
TS_AUTHKEY='…' bash /workspace/user_data/webide_start.sh
# 若尚未落盘：先从仓拷贝 scripts/hidevlab/webide_start.sh
```

## 白话现状

- **新机已重建**：物理卡是 `/dev/davinci2`（不是旧的 davinci7），加法 NPU 对拍**已绿** → 卡干净。
- **完整仓已传到** `/workspace/ascendc`（含贯通链 EN12、hidevlab 脚本）。
- **Tailscale 节点仍在**：`hidevlab-npu` / `100.70.113.117`（以实机为准）。
- 若 Agent SSH 断：userspace 入站靠 `serve tcp:2222`；优先用 `agent_connect.sh` / 人侧重跑 `webide_start.sh`，**不要**整段重做无关入网。

## 硬约束

- 只做**新贯通链**（`graph-tests/enc_cann_ntt/`）；**禁止**再跑旧 Encaps 猎挂。
- **禁止**拉看门狗/心跳保活。
- **不动** `cursor/kem-2launch-sticky-1534`；基准只认 `fe53`。

## P0

1. 人侧 WebIDE：`webide_start.sh` 见到 `READY` / `ts_ip=100.…`。  
2. Agent：`bash scripts/hidevlab/agent_connect.sh` 探活成功。  
3. 再继续 EN12 CPU→NPU 贯通实验。

短修复（仅 sshd/serve，勿整段重入网）仍可用：

```bash
TS=/workspace/tailscale/bin/tailscale; SOCK=/workspace/tailscale/run/tailscaled.sock
pkill -f 'sshd.*2222' 2>/dev/null || true
/usr/sbin/sshd -p 2222 -o ListenAddress=127.0.0.1 -o PermitRootLogin=prohibit-password -o PasswordAuthentication=no -o PubkeyAuthentication=yes -o PidFile=/tmp/sshd2222.pid
"$TS" --socket="$SOCK" serve --bg --tcp 2222 tcp://127.0.0.1:2222
"$TS" --socket="$SOCK" serve status | head
```
