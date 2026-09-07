#!/usr/bin/env bash
# CANNLab 开机后一键重连：Tailscale 入网 + 2222 sshd（供 Cursor Agent 免密）+ 空闲看门狗。
# 本文件为仓库内**权威版**（scripts/cannlab/）；工程持久在 /mnt/workspace/ascendc。
#
# 背景：CANNLab 实例 stop/start 后，除持久盘（盘1→/mnt/workspace、盘2→/home）外的系统盘
#   会重置（含 tailscale、sshd、其 host key）。本脚本自愈地把这些重新装好；authorized_keys
#   在 /home（持久）故公钥无需每次重授。tailscale/sshd 装在系统盘，所以每次开机需跑本脚本一次。
#
# 用法（每次控制台“启动”实例后，在 WebIDE 终端跑一次）：
#   TS_AUTHKEY='tskey-auth-xxxx' bash /mnt/workspace/ascendc/scripts/cannlab/agent_bootstrap.sh
# 可选：
#   IDLE_MIN=30                       # 看门狗空闲分钟阈值
#   CURSOR_PUBKEY='ssh-ed25519 ...'   # 覆盖默认授权公钥（换新 Cursor Agent 密钥时用）
set -u
: "${TS_AUTHKEY:?请设置 TS_AUTHKEY（与首次入网同一把 Tailscale key；须 reusable）}"

# 默认授权公钥 = 当前 Cursor Agent 密钥对的公钥（私钥存于 Cloud Secret CANNLAB_SSH_KEY）。
CURSOR_PUBKEY="${CURSOR_PUBKEY:-ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIB4+JMqBJWCNaAyd3fvpTHZ371OhctRYObuG2GUxDDof cursor-agent}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 1) Tailscale：缺则装，起 tailscaled，入网并开 Tailscale SSH
if ! command -v tailscale >/dev/null 2>&1; then curl -fsSL https://tailscale.com/install.sh | sudo bash; fi
sudo systemctl enable --now tailscaled 2>/dev/null || { sudo nohup tailscaled >/tmp/tailscaled.log 2>&1 & sleep 3; }
sudo tailscale up --authkey="${TS_AUTHKEY}" --hostname=cannlab-npu --ssh || \
  sudo tailscale up --authkey="${TS_AUTHKEY}" --hostname=cannlab-npu
TSIP="$(sudo tailscale ip -4 2>/dev/null | head -1)"

# 2) 授权 Cursor 公钥（/home 持久，多为一次性）
mkdir -p ~/.ssh && chmod 700 ~/.ssh
grep -qF "${CURSOR_PUBKEY%% *} ${CURSOR_PUBKEY#* }" ~/.ssh/authorized_keys 2>/dev/null || \
  grep -qF "${CURSOR_PUBKEY}" ~/.ssh/authorized_keys 2>/dev/null || echo "${CURSOR_PUBKEY}" >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys

# 3) 在 Tailscale 网卡地址上起 2222 sshd（22 被 Tailscale SSH 接管，故用 2222）
SSHD="$(command -v sshd || echo /usr/sbin/sshd)"
[ -x "${SSHD}" ] || { sudo apt-get update -qq && sudo apt-get install -y -qq openssh-server; SSHD=/usr/sbin/sshd; }
sudo mkdir -p /run/sshd; sudo ssh-keygen -A 2>/dev/null || true
sudo pkill -f 'sshd .*-p 2222' 2>/dev/null || true; sleep 1
sudo "${SSHD}" -p 2222 -o ListenAddress="${TSIP}" \
  -o PermitRootLogin=no -o PasswordAuthentication=no -o PubkeyAuthentication=yes \
  -o PidFile=/tmp/sshd2222.pid

# 4) 空闲看门狗（防卡时白烧）；用仓库内权威版
pkill -f agent_watchdog.sh 2>/dev/null || true
IDLE_MIN="${IDLE_MIN:-30}" setsid nohup bash "${SCRIPT_DIR}/agent_watchdog.sh" >/dev/null 2>&1 < /dev/null &

echo "===== bootstrap done ====="
echo "ts_ip=${TSIP}  sshd=2222  watchdog=on(IDLE_MIN=${IDLE_MIN:-30})"
ss -ltnp 2>/dev/null | grep ':2222' || echo '!! 2222 未监听'
