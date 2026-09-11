# HiDevLab — 其他 Cursor Agent 如何连服务器

> **给 Cloud / 本地 Cursor Agent 读的短文**。HiDevLab **WebIDE 里常无可用 git**，人侧不靠 `git pull`；  
> **Agent 侧在 Cursor 机器上可以 `git pull` / `git fetch` 本仓库拿到脚本。**  
> 完整背景见 [`HiDevLab-WebIDE操作手册.md`](HiDevLab-WebIDE操作手册.md) §7.1。  
> **最后刷新**：2026-09-11

---

## 0. 分工（先看清）

| 谁 | 在哪 | 做什么 |
|----|------|--------|
| **人** | HiDevLab **WebIDE 终端** | 每次开机跑 `webide_start.sh`（盒子常 **不能 git**，脚本应在 `/workspace/user_data/`） |
| **Agent** | Cursor Cloud / 本机（本仓库） | `git pull` 后跑 `scripts/hidevlab/agent_connect.sh` 经 Tailscale 连盒子 |

两套 Secret（Dashboard，**reusable**）：

| Secret | 谁用 |
|--------|------|
| `TS_AUTHKEY` / `TAILSCALE_AUTHKEY` | 人侧入网 + Agent 侧入网（可同一把 reusable key） |
| `CANNLAB_SSH_KEY` | **仅 Agent** SSH 私钥（缺头尾 / 被压成单行也没关系，脚本会重建） |

默认节点名：`hidevlab-npu` · 端口：`2222` · 用户：`root`

---

## 1. 人侧（WebIDE，每次开机一条）

盒子 **不要依赖 git**。首次把脚本拷进持久盘后，以后只跑持久盘副本：

```bash
# —— 首次（若仓库树碰巧在 /workspace/ascendc 且已有脚本）——
TS_AUTHKEY='tskey-…' bash /workspace/ascendc/scripts/hidevlab/webide_start.sh

# —— 之后（推荐；overlay 重启也不丢）——
TS_AUTHKEY='tskey-…' bash /workspace/user_data/webide_start.sh
```

成功标志：打印 `===== READY =====`，有 `ts_ip=100.…`，`sshd` 在 `127.0.0.1:2222`。  
把摘要文件贴给 Agent（**勿贴 TS_AUTHKEY**）：

- `/workspace/user_data/hidevlab_tailscale.env`

只起网络、不配 CANN：`SKIP_BOOT=1 TS_AUTHKEY='…' bash /workspace/user_data/webide_start.sh`

---

## 2. 其他 Agent（Cursor 上拉取本仓后执行）

```bash
# 在 Cursor Agent 的仓库根（可以 git）
git fetch origin cursor/hidevlab-webide-start-ffc6
git checkout cursor/hidevlab-webide-start-ffc6
# 或已合入 main 后：git pull

bash scripts/hidevlab/agent_connect.sh
# 跑远程命令：
bash scripts/hidevlab/agent_connect.sh --exec 'hostname; npu-smi info | head'
```

脚本自动：

1. `materialize_ssh_key.sh` 从 `CANNLAB_SSH_KEY` 重建合法私钥 → `~/.ssh/hidevlab`  
2. 本机 **userspace** `tailscaled` + `up`（`TAILSCALE_AUTHKEY` / `TS_AUTHKEY`）  
3. 经 SOCKS `127.0.0.1:1055` SSH 到 `root@hidevlab-npu:2222`

覆盖主机名（若人侧改了 `TS_HOSTNAME`）：

```bash
HIDEVLAB_HOST=实际hostname bash scripts/hidevlab/agent_connect.sh
```

导出可复用 SSH 数组：

```bash
eval "$(bash scripts/hidevlab/agent_connect.sh --export)"
"${HIDEVLAB_SSH[@]}" 'echo OK'
```

---

## 3. 等价手写（排障）

```bash
bash scripts/hidevlab/materialize_ssh_key.sh ~/.ssh/hidevlab
# …确保本机 userspace tailscaled 已 up …
ssh -o ProxyCommand="nc -X 5 -x 127.0.0.1:1055 %h %p" \
    -o StrictHostKeyChecking=accept-new -o IdentitiesOnly=yes \
    -i ~/.ssh/hidevlab -o ConnectTimeout=25 -o ConnectionAttempts=3 \
    -p 2222 root@hidevlab-npu 'echo OK'
```

---

## 4. 连不上时（按序）

1. **人侧没跑 / sshd 掉了**（HiDevLab 空闲关机或手工进程被收）→ WebIDE 再跑 `webide_start.sh`  
2. **`tailscale status` 里没有 hidevlab-npu / offline** → 同上  
3. **Agent 缺 Secret** → Dashboard 检查 `TAILSCALE_AUTHKEY`、`CANNLAB_SSH_KEY`  
4. **`error in libcrypto`** → 不要改 Dashboard；确认跑的是带 `materialize_ssh_key.sh` 的分支  
5. **hostname 不对** → 看人贴的 `ts_hostname=`，设 `HIDEVLAB_HOST`

多 Agent **可同时 SSH**；真机上板建议串行，避免抢同一张卡。

---

## 5. 相关路径（仓库内）

| 路径 | 角色 |
|------|------|
| [`scripts/hidevlab/webide_start.sh`](../../scripts/hidevlab/webide_start.sh) | 人侧一键开机 |
| [`scripts/hidevlab/agent_connect.sh`](../../scripts/hidevlab/agent_connect.sh) | Agent 侧一键连接 |
| [`scripts/hidevlab/materialize_ssh_key.sh`](../../scripts/hidevlab/materialize_ssh_key.sh) | 私钥格式兜底 |
| [`scripts/hidevlab/README.md`](../../scripts/hidevlab/README.md) | 目录说明 |
| 本文件 | **Agent 开任务先读这一页** |
