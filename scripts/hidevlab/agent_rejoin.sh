#!/usr/bin/env bash
# HiDevLab「每次开机」短启动：假定 /workspace/tailscale 静态包已在。
# 首次或丢盘后请先跑 agent_bootstrap_static.sh（或手册分步 A–B）。
#
#   TS_AUTHKEY='…' bash /workspace/ascendc/scripts/hidevlab/agent_rejoin.sh
# 建议：
#   TS_AUTHKEY='…' nohup bash …/agent_rejoin.sh >/workspace/hidevlab_rejoin.log 2>&1 &
set -u
: "${TS_AUTHKEY:?set TS_AUTHKEY}"
TS_HOSTNAME="${TS_HOSTNAME:-hidevlab-npu}"
PUB="${CURSOR_PUBKEY:-ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIB4+JMqBJWCNaAyd3fvpTHZ371OhctRYObuG2GUxDDof cursor-agent}"
WS="${WORKSPACE_ROOT:-/workspace}"
TS_HOME="${TS_HOME:-$WS/tailscale}"
SOCK="$TS_HOME/run/tailscaled.sock"
LOG="${HIDEVLAB_REJOIN_LOG:-$WS/hidevlab_rejoin.log}"

log() { echo "[$(date '+%F %T')] $*" | tee -a "$LOG"; }

if [ ! -x "$TS_HOME/bin/tailscaled" ] || [ ! -x "$TS_HOME/bin/tailscale" ]; then
  log "!! 静态包不在 $TS_HOME/bin — 先跑首次安装（bootstrap_static / 手册 A–B）"
  exit 2
fi
mkdir -p "$TS_HOME/state" "$TS_HOME/run"
export PATH="$TS_HOME/bin:$PATH"

if ! pgrep -f "$TS_HOME/bin/tailscaled" >/dev/null 2>&1; then
  if [ -e /dev/net/tun ]; then
    log "start tailscaled TUN"
    nohup "$TS_HOME/bin/tailscaled" \
      --state="$TS_HOME/state/tailscaled.state" \
      --socket="$SOCK" --port=41641 \
      >"$TS_HOME/tailscaled.log" 2>&1 &
  else
    log "start tailscaled userspace"
    nohup "$TS_HOME/bin/tailscaled" \
      --tun=userspace-networking \
      --state="$TS_HOME/state/tailscaled.state" \
      --socket="$SOCK" \
      --socks5-server=localhost:1055 \
      >"$TS_HOME/tailscaled.log" 2>&1 &
  fi
  sleep 3
else
  log "tailscaled already running"
fi

log "tailscale up"
"$TS_HOME/bin/tailscale" --socket="$SOCK" up \
  --authkey="$TS_AUTHKEY" --hostname="$TS_HOSTNAME" \
  --accept-dns=false --accept-routes=false
TSIP="$("$TS_HOME/bin/tailscale" --socket="$SOCK" ip -4 | head -1)"
[ -n "$TSIP" ] || { log "!! no ipv4"; exit 1; }
log "ts_ip=$TSIP"

mkdir -p ~/.ssh && chmod 700 ~/.ssh
touch ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys
grep -qF "$PUB" ~/.ssh/authorized_keys 2>/dev/null || echo "$PUB" >> ~/.ssh/authorized_keys

SSHD="$(command -v sshd || true)"; [ -x "${SSHD:-}" ] || SSHD=/usr/sbin/sshd
if [ -x "$SSHD" ]; then
  mkdir -p /run/sshd; ssh-keygen -A 2>/dev/null || true
  if [ -f /tmp/sshd2222.pid ]; then
    op=$(cat /tmp/sshd2222.pid 2>/dev/null || true)
    [ -n "$op" ] && kill -0 "$op" 2>/dev/null && { kill "$op" 2>/dev/null || true; sleep 1; }
  fi
  if [ -e /dev/net/tun ]; then
    "$SSHD" -p 2222 -o ListenAddress="$TSIP" \
      -o PermitRootLogin=prohibit-password -o PasswordAuthentication=no \
      -o PubkeyAuthentication=yes -o PidFile=/tmp/sshd2222.pid
  else
    "$SSHD" -p 2222 -o ListenAddress=0.0.0.0 \
      -o PermitRootLogin=prohibit-password -o PasswordAuthentication=no \
      -o PubkeyAuthentication=yes -o PidFile=/tmp/sshd2222.pid
  fi
  log "sshd :2222 ok"
else
  log "!! no sshd; skip"
fi

printf 'ts_hostname=%s\nts_ip=%s\nsshd_port=2222\nssh_user=%s\nboot_time=%s\n' \
  "$TS_HOSTNAME" "$TSIP" "$(id -un)" "$(date -Is)" \
  | tee "$WS/.hidevlab_tailscale.env" | tee -a "$LOG"
log "===== rejoin done ====="
"$TS_HOME/bin/tailscale" --socket="$SOCK" status | head -12 | tee -a "$LOG" || true
