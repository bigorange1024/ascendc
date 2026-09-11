#!/usr/bin/env bash
# HiDevLab 精简 bootstrap。禁 --ssh；默认建议 nohup 跑，避免 WebIDE 终端被网络变更掐死。
# 用法（推荐）：
#   TS_AUTHKEY='…' nohup bash …/agent_bootstrap_min.sh > /workspace/hidevlab_bootstrap.log 2>&1 &
#   sleep 2; tail -f /workspace/hidevlab_bootstrap.log
set -u
: "${TS_AUTHKEY:?set TS_AUTHKEY}"
TS_HOSTNAME="${TS_HOSTNAME:-hidevlab-npu}"
PUB="${CURSOR_PUBKEY:-ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIB4+JMqBJWCNaAyd3fvpTHZ371OhctRYObuG2GUxDDof cursor-agent}"
WS="${WORKSPACE_ROOT:-/workspace}"
LOG="${HIDEVLAB_BOOTSTRAP_LOG:-$WS/hidevlab_bootstrap.log}"
if [ "$(id -u)" -eq 0 ]; then S=""; else S="sudo"; fi

log() { echo "[$(date '+%F %T')] $*" | tee -a "$LOG"; }

log "start min bootstrap hostname=$TS_HOSTNAME"

if ! command -v tailscale >/dev/null 2>&1; then
  log "installing tailscale"
  curl -fsSL https://tailscale.com/install.sh | $S bash
fi
pgrep -x tailscaled >/dev/null 2>&1 || {
  log "starting tailscaled"
  $S systemctl enable --now tailscaled 2>/dev/null || \
    { $S nohup tailscaled >/tmp/tailscaled.log 2>&1 & sleep 3; }
}

# 禁止 --ssh；关掉路由/DNS 接管，尽量保住 WebIDE 原网络
log "tailscale up (vpn only)"
$S tailscale up --authkey="$TS_AUTHKEY" --hostname="$TS_HOSTNAME" \
  --accept-dns=false --accept-routes=false --shields-up=false
TSIP="$($S tailscale ip -4 2>/dev/null | head -1)"
if [ -z "$TSIP" ]; then
  log "!! no ipv4"
  $S tailscale status 2>&1 | tee -a "$LOG" || true
  exit 1
fi
log "ts_ip=$TSIP"

mkdir -p ~/.ssh && chmod 700 ~/.ssh
touch ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys
grep -qF "$PUB" ~/.ssh/authorized_keys 2>/dev/null || echo "$PUB" >> ~/.ssh/authorized_keys

SSHD="$(command -v sshd || true)"
[ -x "${SSHD:-}" ] || SSHD=/usr/sbin/sshd
if [ ! -x "$SSHD" ]; then
  log "openssh-server missing; skip apt in-band (avoid killing WebIDE). install manually then re-run."
  log "manual: apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y openssh-server"
  exit 2
fi
$S mkdir -p /run/sshd
$S ssh-keygen -A 2>/dev/null || true
if [ -f /tmp/sshd2222.pid ]; then
  op=$(cat /tmp/sshd2222.pid 2>/dev/null || true)
  [ -n "$op" ] && kill -0 "$op" 2>/dev/null && { $S kill "$op" 2>/dev/null || true; sleep 1; }
fi
$S "$SSHD" -p 2222 -o ListenAddress="$TSIP" \
  -o PermitRootLogin=prohibit-password -o PasswordAuthentication=no \
  -o PubkeyAuthentication=yes -o PidFile=/tmp/sshd2222.pid
log "sshd 2222 on $TSIP"

printf 'ts_hostname=%s\nts_ip=%s\nsshd_port=2222\nssh_user=%s\nboot_time=%s\n' \
  "$TS_HOSTNAME" "$TSIP" "$(id -un)" "$(date -Is)" | tee "$WS/.hidevlab_tailscale.env" | tee -a "$LOG"
log "===== hidevlab bootstrap done (min) ====="
if ss -ltnp 2>/dev/null | grep -q 2222 || netstat -ltnp 2>/dev/null | grep -q 2222; then
  ss -ltnp 2>/dev/null | grep 2222 | tee -a "$LOG" || true
  exit 0
fi
log "!! 2222 not listening"
exit 1
