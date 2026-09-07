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
# 若会话闲置数十分钟后本节点被摘除（tailscaled 日志 `PollNetMap ... 404: node not found`，
# status 里自己显示 offline），用 --reset 重新注册（authkey 仍在 env）：
#   sudo tailscale --socket=/var/run/tailscale/tailscaled.sock up --reset --authkey="$TAILSCALE_AUTHKEY" --hostname=cursor-agent

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

### 5.1 Encaps / Decaps 粘性挂取证（~10 分钟，Agent 亲自跑）

> 用途：恢复 **main 口径 stable** 后，在单卡 910B3 上**多轮 / 交叉**跑 Encaps、Decaps，亲眼看粘性挂长什么样。  
> 不做正确性结案；挂因只记 Host 文案 / 超时 / 轮次。`FORCE_REBUILD=1` 避免旧二进制假绿。

```bash
# Cursor 侧：前台 SSH（ServerAlive），一次一把刀；CANNLab 工作树先切到目标分支并 pull
SSH='ssh -o ServerAliveInterval=15 -o ProxyCommand="nc -X 5 -x 127.0.0.1:1055 %h %p" -o StrictHostKeyChecking=accept-new -i ~/.ssh/cannlab -p 2222 developer@cannlab-npu'

eval $SSH '"bash -s"' <<'R'
set -euo pipefail
export LD_LIBRARY_PATH=/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:${LD_LIBRARY_PATH:-}
source /home/developer/Ascend/ascend-toolkit/set_env.sh
cd /mnt/workspace/ascendc
git fetch origin && git checkout cursor/kem-2launch-sticky-1534 && git pull --ff-only
export ASCEND_DEVICE_ID=0 CANNLAB=1 CMAKE_BUILD_JOBS=8
export KEM_ENCAPS_FORCE_REBUILD=1 KEM_DECAPS_FORCE_REBUILD=1
ROOT=examples/stable/ml-kem/ml-kem-1024
# 1) Encaps 多轮（墙钟紧则 TOY 式循环；每轮独立 run.sh）
for i in 1 2 3 4 5 6 7; do
  echo "===== ENCAPS round $i ====="
  timeout 900 bash -lc "cd $ROOT/stable-fips203-mlkem-kem-encaps-k4 && bash run.sh -r npu -v Ascend910B3" \
    || { echo "ENCAPS_FAIL_OR_HANG round=$i rc=$?"; break; }
done
# 2) Decaps 多轮
for i in 1 2 3 4 5 6 7; do
  echo "===== DECAPS round $i ====="
  timeout 900 bash -lc "cd $ROOT/stable-fips203-mlkem-kem-decaps-k4 && bash run.sh -r npu -v Ascend910B3" \
    || { echo "DECAPS_FAIL_OR_HANG round=$i rc=$?"; break; }
done
# 3) 交叉：Encaps→Decaps 再 Encaps（各 1～2 轮，看顺序是否触发）
echo "===== CROSS encaps then decaps ====="
timeout 900 bash -lc "cd $ROOT/stable-fips203-mlkem-kem-encaps-k4 && bash run.sh -r npu -v Ascend910B3"
timeout 900 bash -lc "cd $ROOT/stable-fips203-mlkem-kem-decaps-k4 && bash run.sh -r npu -v Ascend910B3"
echo "===== HANG_OBSERVE_DONE ====="
R
```

也可一键：`bash scripts/cannlab/hang_observe_encaps_decaps.sh`（须已能 `ssh cannlab-npu`）。

## 6. 关键坑（务必记住）

| 坑 | 现象 | 处置 |
|----|------|------|
| **设备号** | 卡挂成 `/dev/davinci3`，但 **ACL 逻辑设备号从 0 枚举**；仓库 `npu_device_map.sh` 会按树/节点选 1/2/3 → `aclrtSetDevice` 报 **107001 无效设备** | 单卡实例**必须显式 `ASCEND_DEVICE_ID=0`**（run.sh 会保留显式值）；或 `export CANNLAB=1` / `NPU_SINGLE_CARD=1` 让分卡表默认全 0 |
| **驱动库** | `npu-smi` / ACL 报 `libc_sec.so`/`libdrvdsmi_host.so` 找不到 | 把 `/usr/local/Ascend/driver/lib64{,/driver,/common}` 加进 `LD_LIBRARY_PATH`（recipe 已含） |
| **sshd 只听 loopback** | 首次 `sshd -p 2222` 绑到 `127.0.0.1` | 用 `-o ListenAddress=<tailscale ip>`（bootstrap 已处理） |
| **停机** | `poweroff/halt/shutdown` 无效（容器 PID1=tini，无 systemd）；`kill -9 1` 被内核拦截 | **`sudo kill -TERM 1`**（tini 优雅退出、节点下线）；**权威停计费以控制台“关机/停止”为准** |
| **持久性** | 重启后 tailscale/sshd 消失 | 仅 `/mnt/workspace`、`/home` 持久；每次开机重跑 `agent_bootstrap.sh` |
| **GitHub 抖动** | 偶发 `curl github 000` | 多为瞬时；重试即可，实例出网整体可达（gitcode/pypi/github 均 200） |
| **DERP 延迟/后台挂起** | 经 DERP 中继时 ssh 偏慢；`nohup ... &` 后台跑 `run.sh` 会让 ssh 通道**迟迟不返回** | **前台**跑 `run.sh`（加 `-o ServerAliveInterval=15`），用 `timeout` 兜底；不要在同一 ssh 里后台化再 tail |
| **闲置节点被摘除** | 会话闲置后**本机** tailscale 节点被摘（`404 node not found`），连 CANNLab 报 SOCKS 失败 | Cursor 侧 `up --reset --authkey="$TAILSCALE_AUTHKEY"` 重注册（见 §4）；新起的 Agent 首次 `up` 不受影响 |
| **停机后控制台“异常”** | `sudo kill -TERM 1` / 看门狗停机是**信号杀 tini**，绕过平台停止流程，控制台常显示 **“异常/error”** 而非干净“已停止” | 属预期副作用；**到控制台手动“关机/停止”再确认一次**（权威停计费）。故 SIGTERM/看门狗只当兜底，日常优先控制台关机 |

## 7. 停卡时（三选一）

1. **控制台“关机/停止”**——唯一权威停止计费方式，也是**唯一让实例状态干净**的方式。
2. `sudo kill -TERM 1`（Agent 或 WebIDE）——让容器退出、节点下线；但这是信号杀 tini，绕过平台停止流程，**控制台事后常显示“异常/error”，需人工再点一次“关机/停止”确认**。
3. **空闲看门狗**（`scripts/cannlab/agent_watchdog.sh`，bootstrap 默认拉起，`IDLE_MIN=30`）——无算力活动超时后自动执行第 2 步；同样会留“异常”态，仍需人工到控制台收尾确认。

> 定位：SIGTERM/看门狗只是“人不在场时先把算力空转掐掉”的**兜底**；日常收工请**优先走控制台“关机/停止”**。

## 8. 已验证证据（2026-09-07, 910B3）

- `ascendc-tests/add_custom`：`-r npu` 上板对拍 `[SUCCESS] output matches golden (Ascend910B3)`。
- `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4`：真机两 launch
  （`f203_keygen_prep` + `mmad_custom`），`ek_kem`=1568B / `dk_kem`=3168B 与 python 参考 `max=0` 逐字节一致，`[SUCCESS]`。
- **冷启动全链自检（2026-09-07 复跑）**：全新 Cursor 会话 → `up --reset` 重注册 → `ssh cannlab-npu:2222`（新实例卡槽 `davinci6`，`ASCEND_DEVICE_ID=0`）→ 仓库 `git pull` 命中 `scripts/cannlab/`+本文档 → KeyGen 真机 `[verify] KEM KeyGen overall PASS` → `sudo kill -TERM 1` 停机、节点下线。整套 handoff 闭环可用。

## 9. 多 Agent 并发约定（单卡实例）

一台单卡实例（1×910B3）上，**连接能并发，但“干活”应串行**——瓶颈不在能否连上，而在单卡 + 共用工作树。

| 层 | 并发 | 说明 |
|----|------|------|
| 连接（ssh/tailnet） | ✅ 可多个 | sshd 支持多会话，tailnet 每 Agent 一个节点；多个 Cursor Agent 同时 `ssh cannlab-npu:2222` 没问题 |
| 工作树 `/mnt/workspace/ascendc` | ❌ 会互踩 | 默认同一 git 工作树，多方同时 `git pull`/改文件/build 到同一 `out_prod_npu/`、`input/`、`output/`、`dump/` 会互相覆盖，对拍不可信 |
| NPU 算力（1×910B3，ACL 设备仅 0） | ⚠️ 不建议 | 多进程同时上板争 AICore/HBM、dump/profiling 路径冲突；本仓 Rule 禁并行多路 SIM，真机同理 |

**默认：一次一个 Agent 跑用例。** 需要多 Agent 时按下述缓解，但**卡是硬瓶颈、跑真机仍要排队**：

1. **各用独立工作树**：每 Agent `git clone` 到自己目录（如 `/mnt/workspace/ascendc-<agentX>`）或 `git worktree`，避免构建/对拍产物互相覆盖。
2. **上板段加锁串行**：跑真机用 `flock /mnt/workspace/.npu.lock -c '... run.sh -r npu ...'`，占卡那一段串起来，谁抢到谁跑、别人等。
3. **hostname 唯一**：多个 Cursor Agent 若都 `--hostname=cursor-agent`，MagicDNS 会重名自动加 `-1`；给每个 Agent 用唯一 hostname 或直接连 IP。

**并发副作用（重要）**：**停机是全局的**——`sudo kill -TERM 1` 会停整台容器，连着的所有 Agent 一起断、正在跑的用例被打断。多 Agent 场景下**别随手 `kill -TERM 1`**，交给空闲看门狗或人工控制台收尾，避免误伤他人任务。
