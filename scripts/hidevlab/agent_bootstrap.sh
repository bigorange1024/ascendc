#!/usr/bin/env bash
# HiDevLab 开机后一键：Tailscale 入网（仅 VPN）+ 2222 sshd（供 Cursor Agent 免密长连）。
# 与 scripts/cannlab/agent_bootstrap.sh 同构；路径/用户/hostname 按 HiDevLab 锁定。
#
# !! 禁止 tailscale up --ssh：会抢 22，HiDevLab WebIDE（VS Code）终端常直接崩
#    （见 https://aka.ms/vscode-troubleshoot-terminal-launch）。本脚本只用 2222 OpenSSH。
#
# 用法（WebIDE 终端，每次环境「运行中」后一次）：
#   TS_AUTHKEY='tskey-auth-xxxx' bash /workspace/ascendc/scripts/hidevlab/agent_bootstrap.sh
# 若仓库脚本尚不在机上，先：
#   bash /workspace/hidevlab_install_bootstrap.sh   # 由 webide_drop_bootstrap.sh 生成
# 再：
#   TS_AUTHKEY='…' bash /workspace/ascendc/scripts/hidevlab/agent_bootstrap.sh
#
# 可选：
#   TS_HOSTNAME=hidevlab-npu
#   CURSOR_PUBKEY='ssh-ed25519 …'
#   ENABLE_WATCHDOG=1   # 默认不拉看门狗；需要自停再显式开
#   IDLE_MIN=30
#   SKIP_WATCHDOG=1     # 显式禁用（与默认一致）
set -u
: "${TS_AUTHKEY:?请设置 TS_AUTHKEY（与 CANNLab 同一把 reusable Tailscale authkey）}"

TS_HOSTNAME="${TS_HOSTNAME:-hidevlab-npu}"
CURSOR_PUBKEY="${CURSOR_PUBKEY:-ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIB4+JMqBJWCNaAyd3fvpTHZ371OhctRYObuG2GUxDDof cursor-agent}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="${WORKSPACE_ROOT:-/workspace}"
LOG="${HIDEVLAB_BOOTSTRAP_LOG:-${WORKSPACE_ROOT}/hidevlab_bootstrap.log}"

if [ "$(id -u)" -eq 0 ]; then SUDO=""; else SUDO="sudo"; fi

log() { echo "[$(date '+%F %T')] $*" | tee -a "${LOG}"; }

log "===== hidevlab agent_bootstrap start ====="
log "hostname_want=${TS_HOSTNAME} user=$(id -un) pwd=${PWD}"

# 1) 安装 / 启动 tailscaled（不 --ssh）
if ! command -v tailscale >/dev/null 2>&1; then
  log "installing tailscale…"
  curl -fsSL https://tailscale.com/install.sh | ${SUDO} bash
fi
if ! pgrep -x tailscaled >/dev/null 2>&1; then
  log "starting tailscaled…"
  ${SUDO} systemctl enable --now tailscaled 2>/dev/null || \
    { ${SUDO} nohup tailscaled >/tmp/tailscaled.log 2>&1 & sleep 3; }
fi

# 明确不加 --ssh，避免掐 WebIDE
log "tailscale up (VPN only, no --ssh)…"
${SUDO} tailscale up --authkey="${TS_AUTHKEY}" --hostname="${TS_HOSTNAME}" --accept-dns=false
TSIP="$(${SUDO} tailscale ip -4 2>/dev/null | head -1)"
if [ -z "${TSIP}" ]; then
  log "!! no IPv4; dump status"
  ${SUDO} tailscale status 2>&1 | tee -a "${LOG}" || true
  exit 1
fi
log "ts_ip=${TSIP}"

# 2) 授权 Cursor 公钥
mkdir -p ~/.ssh && chmod 700 ~/.ssh
touch ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
if ! grep -qF "${CURSOR_PUBKEY}" ~/.ssh/authorized_keys 2>/dev/null; then
  echo "${CURSOR_PUBKEY}" >> ~/.ssh/authorized_keys
  log "authorized_keys: appended cursor-agent pubkey"
else
  log "authorized_keys: pubkey already present"
fi

# 3) OpenSSH 监听 Tailscale IP:2222（不动 22，保护 WebIDE）
SSHD="$(command -v sshd || echo /usr/sbin/sshd)"
if [ ! -x "${SSHD}" ]; then
  log "apt install openssh-server…"
  ${SUDO} apt-get update -qq
  ${SUDO} DEBIAN_FRONTEND=noninteractive apt-get install -y -qq openssh-server
  SSHD=/usr/sbin/sshd
fi
${SUDO} mkdir -p /run/sshd
${SUDO} ssh-keygen -A 2>/dev/null || true
if [ -f /tmp/sshd2222.pid ]; then
  oldpid="$(cat /tmp/sshd2222.pid 2>/dev/null || true)"
  if [ -n "${oldpid}" ] && kill -0 "${oldpid}" 2>/dev/null; then
    ${SUDO} kill "${oldpid}" 2>/dev/null || true
    sleep 1
  fi
fi
${SUDO} "${SSHD}" -p 2222 -o ListenAddress="${TSIP}" \
  -o PermitRootLogin=prohibit-password -o PasswordAuthentication=no \
  -o PubkeyAuthentication=yes -o PidFile=/tmp/sshd2222.pid
log "sshd :2222 on ${TSIP}"

# 4) 看门狗（默认关闭）。历史事故：默认拉起 + 误判 active → 空转近 10h 仍不自停。
# 需要时显式：ENABLE_WATCHDOG=1 IDLE_MIN=30 bash agent_bootstrap.sh …
if [ "${ENABLE_WATCHDOG:-0}" = "1" ] && [ "${SKIP_WATCHDOG:-1}" != "1" ] \
  && [ -f "${SCRIPT_DIR}/agent_watchdog.sh" ]; then
  if [ -f /tmp/hidevlab_watchdog.pid ]; then
    wpid="$(cat /tmp/hidevlab_watchdog.pid 2>/dev/null || true)"
    if [ -n "${wpid}" ] && kill -0 "${wpid}" 2>/dev/null; then
      kill "${wpid}" 2>/dev/null || true
    fi
  fi
  IDLE_MIN="${IDLE_MIN:-30}" setsid nohup bash "${SCRIPT_DIR}/agent_watchdog.sh" \
    >/dev/null 2>&1 < /dev/null &
  echo $! > /tmp/hidevlab_watchdog.pid
  log "watchdog pid=$(cat /tmp/hidevlab_watchdog.pid) IDLE_MIN=${IDLE_MIN:-30}"
else
  log "watchdog skipped (default off; set ENABLE_WATCHDOG=1 to opt-in)"
fi

{
  echo "ts_hostname=${TS_HOSTNAME}"
  echo "ts_ip=${TSIP}"
  echo "sshd_port=2222"
  echo "ssh_user=$(id -un)"
  echo "boot_time=$(date -Is)"
  echo "note=no_tailscale_ssh_keep_webide_port22"
} | tee "${WORKSPACE_ROOT}/.hidevlab_tailscale.env" | tee -a "${LOG}"

log "===== hidevlab bootstrap done ====="
log "Agent: source scripts/hidevlab/lib_ssh.sh && hidevlab_pick_host && hidevlab_ssh_try 'echo OK'"
if ss -ltnp 2>/dev/null | grep -q ':2222'; then
  ss -ltnp 2>/dev/null | grep ':2222' | tee -a "${LOG}"
elif netstat -ltnp 2>/dev/null | grep -q ':2222'; then
  netstat -ltnp 2>/dev/null | grep ':2222' | tee -a "${LOG}"
else
  log "!! 2222 not listening"
  exit 1
fi
