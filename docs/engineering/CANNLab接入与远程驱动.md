# CANNLab 真机接入与远程驱动（Cursor Agent ↔ Tailscale ↔ GitCode CANNLab NPU）

> **用途**：让 Cursor（本地或 Cloud）Agent **全程免密**驱动 GitCode CANNLab 的**真机昇腾 NPU**（910B3），
> 在其上编译、上板、对拍 AscendC 用例，并可控地停止卡时消耗。
> **首次打通**：2026-09-07（910B3；add_custom 与 ML-KEM-1024 KeyGen 均真机 `verify PASS`）。
> 相关：[NPU真机环境说明.md](NPU真机环境说明.md)（三环境/借入机通用）、脚本 [`scripts/cannlab/`](../../scripts/cannlab/)。

---

## 1. 架构

```
Cursor Agent VM                         GitCode CANNLab（Docker 容器, 真机 910B3）
  tailscaled(userspace)                    tailscaled(kernel) + Tailscale SSH
  SOCKS5 127.0.0.1:1055  ───tailnet(DERP)──►  100.x  ◄── sshd 监听 100.x:2222(密钥)
  ssh -o ProxyCommand=nc..SOCKS.. cannlab-npu:2222   →  developer@容器
```

- CANNLab 从实例内**连不出**去主动连 Cursor，且是 NAT 后容器；用 **Tailscale** 组私网,双向可达。
- CANNLab 的 22 端口被 Tailscale SSH 接管，故我们的 OpenSSH 另起在 **2222**。
- Cursor Agent VM 用 **userspace** 模式（无 root TUN），经其 **SOCKS5 代理**连到 tailnet 名字 `cannlab-npu`。

## 2. 前置：两个 Cloud Secret

| Secret | 值 | 说明 |
|--------|----|------|
| `TAILSCALE_AUTHKEY` | `tskey-auth-...` | tailnet 入网 key，**必须 reusable**（Cursor 侧与 CANNLab 侧共用一把，多个 Agent 复用）；建议 ephemeral（离线自动摘除） |
| `CANNLAB_SSH_KEY` | OpenSSH ed25519 **私钥**全文 | 新 Agent 免密连 CANNLab；对应公钥已在 CANNLab 持久 `/home` 的 `authorized_keys`。**只放 Secret，禁止入库** |

> 换新密钥：新 Agent 生成密钥对，把公钥经 `CURSOR_PUBKEY=... bash agent_bootstrap.sh` 写入 CANNLab，私钥存 `CANNLAB_SSH_KEY`。

## 3. CANNLab 侧（网页控制台 + WebIDE，人工一次/开机）

1. 控制台 **启动** 实例（规格：`1*NPU 910B3, 32vCPUs, 32GiB`；盘1→`/mnt/workspace`、盘2→`/home` 均持久 EVS）。
2. WebIDE 终端跑一次（自愈装 tailscale、起 2222 sshd、授权公钥、拉起看门狗）：
   ```bash
   TS_AUTHKEY='tskey-你的key' bash /mnt/workspace/ascendc/scripts/cannlab/agent_bootstrap.sh
   ```
   看到 `2222` 在 `100.x` 监听即成功。工程持久在 `/mnt/workspace/ascendc`（`git pull` 取最新）。

> 为何需要人工这一步：tailscale/sshd 装在**系统盘**，实例重启会重置（`/mnt/workspace`、`/home` 不受影响）；
> 且必须先有网+sshd，Agent 才连得进（先有鸡才有蛋）。

## 4. Cursor / 新 Agent 侧（自动）

```bash
# 1) 本机装 tailscale(userspace)+netcat，入网
command -v tailscale >/dev/null || curl -fsSL https://tailscale.com/install.sh | sudo bash
command -v nc >/dev/null || sudo apt-get install -y -qq netcat-openbsd
sudo tailscaled --tun=userspace-networking --socks5-server=localhost:1055 \
  --outbound-http-proxy-listen=localhost:1080 --state=/var/lib/tailscale/tailscaled.state \
  --socket=/var/run/tailscale/tailscaled.sock >/tmp/tailscaled.log 2>&1 &
sleep 3
sudo tailscale --socket=/var/run/tailscale/tailscaled.sock up --authkey="$TAILSCALE_AUTHKEY" --hostname=cursor-agent

# 2) 写入 CANNLab 私钥
mkdir -p ~/.ssh && chmod 700 ~/.ssh
printf '%s\n' "$CANNLAB_SSH_KEY" > ~/.ssh/cannlab && chmod 600 ~/.ssh/cannlab

# 3) 连接（用 hostname，IP 会变）
SSH='ssh -o ProxyCommand="nc -X 5 -x 127.0.0.1:1055 %h %p" -o StrictHostKeyChecking=accept-new -i ~/.ssh/cannlab -p 2222 developer@cannlab-npu'
eval $SSH '"echo CONNECTED pid1=$(cat /proc/1/comm)"'
```

## 5. 跑真机用例（固定 recipe）

```bash
eval $SSH '"bash -s"' <<'R'
export LD_LIBRARY_PATH=/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:$LD_LIBRARY_PATH
source /home/developer/Ascend/ascend-toolkit/set_env.sh
cd /mnt/workspace/ascendc && git pull --ff-only
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4
ASCEND_DEVICE_ID=0 CMAKE_BUILD_JOBS=16 bash run.sh -r npu -v Ascend910B3
R
```

期望尾部：`[verify] KEM KeyGen overall PASS` + `[SUCCESS] ... (npu)`。

## 6. 关键坑（务必记住）

| 坑 | 现象 | 处置 |
|----|------|------|
| **设备号** | 卡挂成 `/dev/davinci3`，但 **ACL 逻辑设备号从 0 枚举**；仓库 `npu_device_map.sh` 会按树/节点选 1/2/3 → `aclrtSetDevice` 报 **107001 无效设备** | 单卡实例**必须显式 `ASCEND_DEVICE_ID=0`**（run.sh 会保留显式值） |
| **驱动库** | `npu-smi` / ACL 报 `libc_sec.so`/`libdrvdsmi_host.so` 找不到 | 把 `/usr/local/Ascend/driver/lib64{,/driver,/common}` 加进 `LD_LIBRARY_PATH`（recipe 已含） |
| **sshd 只听 loopback** | 首次 `sshd -p 2222` 绑到 `127.0.0.1` | 用 `-o ListenAddress=<tailscale ip>`（bootstrap 已处理） |
| **停机** | `poweroff/halt/shutdown` 无效（容器 PID1=tini，无 systemd）；`kill -9 1` 被内核拦截 | **`sudo kill -TERM 1`**（tini 优雅退出、节点下线）；**权威停计费以控制台“关机/停止”为准** |
| **持久性** | 重启后 tailscale/sshd 消失 | 仅 `/mnt/workspace`、`/home` 持久；每次开机重跑 `agent_bootstrap.sh` |
| **GitHub 抖动** | 偶发 `curl github 000` | 多为瞬时；重试即可，实例出网整体可达（gitcode/pypi/github 均 200） |

## 7. 停卡时（三选一）

1. **控制台“关机/停止”**——唯一权威停止计费方式。
2. `sudo kill -TERM 1`（Agent 或 WebIDE）——让容器退出、节点下线；计费是否停仍看控制台。
3. **空闲看门狗**（`scripts/cannlab/agent_watchdog.sh`，bootstrap 默认拉起，`IDLE_MIN=30`）——无算力活动超时后自动执行第 2 步。

## 8. 已验证证据（2026-09-07, 910B3）

- `ascendc-tests/add_custom`：`-r npu` 上板对拍 `[SUCCESS] output matches golden (Ascend910B3)`。
- `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4`：真机两 launch
  （`f203_keygen_prep` + `mmad_custom`），`ek_kem`=1568B / `dk_kem`=3168B 与 python 参考 `max=0` 逐字节一致，`[SUCCESS]`。
