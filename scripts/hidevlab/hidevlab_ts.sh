#!/usr/bin/env bash
# HiDevLab Tailscale 一键（人只跑这一条）。
#
#   TS_AUTHKEY='…' bash /workspace/hidevlab_ts.sh
#
# 行为：
#   - 无静态包 → 下载 + userspace daemon + up + sshd:2222 + serve
#   - 已有静态包 → 只起 daemon + up + sshd + serve（不重下）
# 硬禁：apt 装 tailscale、tailscale up --ssh（会崩 WebIDE）
set -u
: "${TS_AUTHKEY:?请设置 TS_AUTHKEY（不要贴到聊天；只在本机终端导出）}"

TS_HOSTNAME="${TS_HOSTNAME:-hidevlab-npu}"
PUB="${CURSOR_PUBKEY:-ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIB4+JMqBJWCNaAyd3fvpTHZ371OhctRYObuG2GUxDDof cursor-agent}"
WS="${WORKSPACE_ROOT:-/workspace}"
TS_HOME="${TS_HOME:-$WS/tailscale}"
SOCK="$TS_HOME/run/tailscaled.sock"
LOG="${HIDEVLAB_TS_LOG:-$WS/hidevlab_ts.log}"
mkdir -p "$TS_HOME/bin" "$TS_HOME/state" "$TS_HOME/run" "$WS"

log() { echo "[$(date '+%F %T')] $*" | tee -a "$LOG"; }

arch="$(uname -m)"
case "$arch" in
  aarch64|arm64) TS_ARCH=arm64 ;;
  x86_64|amd64)  TS_ARCH=amd64 ;;
  *) log "!! unsupported arch=$arch"; exit 1 ;;
esac

# ---------- 1) 静态包（仅缺失时下载）----------
if [ ! -x "$TS_HOME/bin/tailscale" ] || [ ! -x "$TS_HOME/bin/tailscaled" ]; then
  log "FIRST: download static tailscale ($TS_ARCH)"
  tg="$TS_HOME/ts.tgz"
  curl -fL "https://pkgs.tailscale.com/stable/tailscale_latest_${TS_ARCH}.tgz" -o "$tg"
  rm -rf "$TS_HOME/extract"
  mkdir -p "$TS_HOME/extract"
  tar -xzf "$tg" -C "$TS_HOME/extract"
  src="$(find "$TS_HOME/extract" -maxdepth 1 -type d -name 'tailscale_*' | head -1)"
  [ -n "$src" ] || { log "!! extract failed"; exit 1; }
  cp -f "$src/tailscale" "$src/tailscaled" "$TS_HOME/bin/"
  chmod +x "$TS_HOME/bin/tailscale" "$TS_HOME/bin/tailscaled"
  log "static ok: $($TS_HOME/bin/tailscale version | head -1)"
else
  log "REJOIN: static binaries present, skip download"
fi
export PATH="$TS_HOME/bin:$PATH"
TS="$TS_HOME/bin/tailscale"
TSD="$TS_HOME/bin/tailscaled"

# ---------- 2) userspace daemon（本镜像无 /dev/net/tun）----------
if ! pgrep -f "$TS_HOME/bin/tailscaled" >/dev/null 2>&1; then
  log "start tailscaled (userspace)"
  nohup "$TSD" \
    --tun=userspace-networking \
    --state="$TS_HOME/state/tailscaled.state" \
    --socket="$SOCK" \
    --socks5-server=localhost:1055 \
    >"$TS_HOME/tailscaled.log" 2>&1 &
  sleep 3
else
  log "tailscaled already running"
fi
if ! pgrep -f "$TS_HOME/bin/tailscaled" >/dev/null 2>&1; then
  log "!! tailscaled failed; dump log"
  tail -30 "$TS_HOME/tailscaled.log" 2>/dev/null | tee -a "$LOG" || true
  exit 1
fi

# ---------- 3) 入网（禁 --ssh）----------
log "tailscale up hostname=$TS_HOSTNAME"
"$TS" --socket="$SOCK" up \
  --authkey="$TS_AUTHKEY" --hostname="$TS_HOSTNAME" \
  --accept-dns=false --accept-routes=false
TSIP="$("$TS" --socket="$SOCK" ip -4 | head -1)"
[ -n "$TSIP" ] || {
  log "!! no ipv4"
  "$TS" --socket="$SOCK" status 2>&1 | tee -a "$LOG" || true
  exit 1
}
log "ts_ip=$TSIP"

# ---------- 4) Cursor 公钥 ----------
mkdir -p ~/.ssh && chmod 700 ~/.ssh
touch ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys
grep -qF "$PUB" ~/.ssh/authorized_keys 2>/dev/null || echo "$PUB" >> ~/.ssh/authorized_keys

# ---------- 5) 本机 sshd :2222（只听 127.0.0.1，不碰 22）----------
SSHD="$(command -v sshd || true)"
[ -x "${SSHD:-}" ] || SSHD=/usr/sbin/sshd
if [ ! -x "$SSHD" ]; then
  log "!! 无 sshd，无法给 Agent 开壳（本镜像勿 apt 乱装）。请换带 openssh-server 的镜像或手工装好后再跑。"
  exit 2
fi
mkdir -p /run/sshd
ssh-keygen -A 2>/dev/null || true
if [ -f /tmp/sshd2222.pid ]; then
  op=$(cat /tmp/sshd2222.pid 2>/dev/null || true)
  [ -n "$op" ] && kill -0 "$op" 2>/dev/null && { kill "$op" 2>/dev/null || true; sleep 1; }
fi
"$SSHD" -p 2222 -o ListenAddress=127.0.0.1 \
  -o PermitRootLogin=prohibit-password -o PasswordAuthentication=no \
  -o PubkeyAuthentication=yes -o PidFile=/tmp/sshd2222.pid
log "sshd 127.0.0.1:2222 ok"

# ---------- 6) serve：userspace 入站靠这个 ----------
"$TS" --socket="$SOCK" serve --bg --tcp 2222 tcp://127.0.0.1:2222 >/dev/null 2>&1 || \
  "$TS" --socket="$SOCK" serve --bg --tcp=2222 tcp://127.0.0.1:2222
log "serve tcp:2222 ok"

# ---------- 7) 摘要 ----------
{
  echo "ts_hostname=$TS_HOSTNAME"
  echo "ts_ip=$TSIP"
  echo "sshd_port=2222"
  echo "ssh_user=$(id -un)"
  echo "mode=userspace+serve"
  echo "ts_home=$TS_HOME"
  echo "boot_time=$(date -Is)"
} | tee "$WS/.hidevlab_tailscale.env" | tee -a "$LOG"

log "===== hidevlab_ts done ====="
"$TS" --socket="$SOCK" status | head -15 | tee -a "$LOG" || true
"$TS" --socket="$SOCK" serve status 2>&1 | head -12 | tee -a "$LOG" || true
echo
echo "把 /workspace/.hidevlab_tailscale.env 内容贴回 Agent 即可（不要贴 TS_AUTHKEY）。"
