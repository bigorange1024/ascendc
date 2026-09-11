# scripts/hidevlab — HiDevLab（昇腾在线开发）辅助

与 GitCode CANNLab（`scripts/cannlab/`）**分开维护**。

| 文件 | 作用 |
|------|------|
| **[`webide_start.sh`](webide_start.sh)** | **人侧每次开机只跑这一条**：CANN 环境 + userspace Tailscale + loopback sshd:2222；并拷到 `/workspace/user_data/` 持久 |
| [`hidevlab_ts.sh`](hidevlab_ts.sh) | 仅 Tailscale/sshd（被 `webide_start.sh` 调用；也可单独跑） |
| [`webide_boot.sh`](webide_boot.sh) | 只配 CANN/`LD_LIBRARY_PATH`/设备号、体检 |
| [`webide_recipe.sh`](webide_recipe.sh) | 打印可贴进 WebIDE 的真机配方（不 SSH） |
| [`materialize_ssh_key.sh`](materialize_ssh_key.sh) | 从 `CANNLAB_SSH_KEY`（可缺头尾/被压成单行）重建合法私钥并自证 |
| [`agent_connect.sh`](agent_connect.sh) | **Agent 侧**一键：materialize 私钥 + 本机 userspace tailscale + 探活/执行远程命令 |

权威操作手册：[`docs/engineering/HiDevLab-WebIDE操作手册.md`](../../docs/engineering/HiDevLab-WebIDE操作手册.md)。

## 人侧（WebIDE 终端，每次开机）

```bash
# 首次（仓库在 /workspace/ascendc）：
TS_AUTHKEY='tskey-auth-…' bash /workspace/ascendc/scripts/hidevlab/webide_start.sh

# 之后（脚本已拷到持久盘，不依赖 overlay/git）：
TS_AUTHKEY='tskey-auth-…' bash /workspace/user_data/webide_start.sh
```

看到 `===== READY =====` 且打印 `ts_ip=100.…` 即成功。把 env 摘要贴回 Agent（**勿贴 TS_AUTHKEY**）。

## Agent 侧

```bash
bash scripts/hidevlab/agent_connect.sh
# 或跑一条远程命令：
bash scripts/hidevlab/agent_connect.sh --exec 'npu-smi info | head'
```

需要 Secret：`TAILSCALE_AUTHKEY`（或 `TS_AUTHKEY`）、`CANNLAB_SSH_KEY`（格式坏了也没关系，会自动 materialize）。
