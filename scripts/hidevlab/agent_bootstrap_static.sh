#!/usr/bin/env bash
# HiDevLab：用 Tailscale **静态包**入网（不走 apt，避开镜像依赖锁死）。
# 禁 --ssh；建议 nohup 跑。
#
#   TS_AUTHKEY='…' nohup bash agent_bootstrap_static.sh >/workspace/hidevlab_bootstrap.log 2>&1 &
set -u
: "${TS_AUTHKEY:?set TS_AUTHKEY}"
TS_HOSTNAME="${TS_HOSTNAME:-hidevlab-npu}"
PUB="${CURSOR_PUBKEY:-ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIB4+JMqBJWCNaAyd3fvpTHZ371OhctRYObuG2GUxDDof cursor-agent}"
WS="${WORKSPACE_ROOT:-/workspace}"
TS_HOME="${TS_HOME:-$WS/tailscale}"
LOG="${HIDEVLAB_BOOTSTRAP_LOG:-$WS/hidevlab_bootstrap.log}"
mkdir -p "$TS_HOME/bin" "$TS_HOME/state" "$TS_HOME/run"

log() { echo "[$(date '+%F %T')] $*" | tee -a "$LOG"; }

arch="$(uname -m)"
case "$arch" in
  aarch64|arm64) TS_ARCH=arm64 ;;
  x86_64|amd64)  TS_ARCH=amd64 ;;
  *) log "!! unsupported arch=$arch"; exit 1 ;;
esac

# 1) 静态包
if [ ! -x "$TS_HOME/bin/tailscale" ] || [ ! -x "$TS_HOME/bin/tailscaled" ]; then
  log "download static tailscale ($TS_ARCH)"
  tg="$TS_HOME/ts.tgz"
  curl -fL "https://pkgs.tailscale.com/stable/tailscale_latest_${TS_ARCH}.tgz" -o "$tg"
  rm -rf "$TS_HOME/extract"
  mkdir -p "$TS_HOME/extract"
  tar -xzf "$tg" -C "$TS_HOME/extract"
  src="$(find "$TS_HOME/extract" -maxdepth 1 -type d -name 'tailscale_*' | head -1)"
  [ -n "$src" ] || { log "!! extract failed"; exit 1; }
  cp -f "$src/tailscale" "$src/tailscaled" "$TS_HOME/bin/"
  chmod +x "$TS_HOME/bin/tailscale" "$TS_HOME/bin/tailscaled"
  log "static binaries ready: $($TS_HOME/bin/tailscale version | head -1)"
fi
export PATH="$TS_HOME/bin:$PATH"

# 2) 起 daemon：优先 kernel TUN；无 /dev/net/tun 则 userspace（入站靠 DERP，仍起 2222）
if ! pgrep -f "$TS_HOME/bin/tailscaled" >/dev/null 2>&1; then
  if [ -e /dev/net/tun ]; then
    log "starting tailscaled (TUN)"
    nohup "$TS_HOME/bin/tailscaled" \
      --state="$TS_HOME/state/tailscaled.state" \
      --socket="$TS_HOME/run/tailscaled.sock" \
      --port=41641 \
      >"$TS_HOME/tailscaled.log" 2>&1 &
  else
    log "starting tailscaled (userspace, no /dev/net/tun)"
    nohup "$TS_HOME/bin/tailscaled" \
      --tun=userspace-networking \
      --state="$TS_HOME/state/tailscaled.state" \
      --socket="$TS_HOME/run/tailscaled.sock" \
      --socks5-server=localhost:1055 \
      >"$TS_HOME/tailscaled.log" 2>&1 &
  fi
  sleep 3
fi
SOCK="$TS_HOME/run/tailscaled.sock"

log "tailscale up (vpn only, no --ssh)"
"$TS_HOME/bin/tailscale" --socket="$SOCK" up \
  --authkey="$TS_AUTHKEY" --hostname="$TS_HOSTNAME" \
  --accept-dns=false --accept-routes=false
TSIP="$("$TS_HOME/bin/tailscale" --socket="$SOCK" ip -4 | head -1)"
[ -n "$TSIP" ] || {
  log "!! no ipv4"
  "$TS_HOME/bin/tailscale" --socket="$SOCK" status 2>&1 | tee -a "$LOG" || true
  exit 1
}
log "ts_ip=$TSIP"

# 3) 公钥
mkdir -p ~/.ssh && chmod 700 ~/.ssh
touch ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys
grep -qF "$PUB" ~/.ssh/authorized_keys 2>/dev/null || echo "$PUB" >> ~/.ssh/authorized_keys

# 4) sshd :2222（有二进制才起；不 apt）
SSHD="$(command -v sshd || true)"
[ -x "${SSHD:-}" ] || SSHD=/usr/sbin/sshd
if [ -x "$SSHD" ]; then
  mkdir -p /run/sshd
  ssh-keygen -A 2>/dev/null || true
  if [ -f /tmp/sshd2222.pid ]; then
    op=$(cat /tmp/sshd2222.pid 2>/dev/null || true)
    [ -n "$op" ] && kill -0 "$op" 2>/dev/null && { kill "$op" 2>/dev/null || true; sleep 1; }
  fi
  # userspace 时 ListenAddress=TSIP 可能无效 → 同时听 0.0.0.0:2222（仅钥、禁密码）
  if [ -e /dev/net/tun ]; then
    "$SSHD" -p 2222 -o ListenAddress="$TSIP" \
      -o PermitRootLogin=prohibit-password -o PasswordAuthentication=no \
      -o PubkeyAuthentication=yes -o PidFile=/tmp/sshd2222.pid
  else
    "$SSHD" -p 2222 -o ListenAddress=0.0.0.0 \
      -o PermitRootLogin=prohibit-password -o PasswordAuthentication=no \
      -o PubkeyAuthentication=yes -o PidFile=/tmp/sshd2222.pid
    log "userspace mode: sshd on 0.0.0.0:2222 (tailnet path via DERP/NAT may need Agent SOCKS)"
  fi
  log "sshd started"
else
  log "!! no sshd binary; skip (do not apt). Agent may use Tailscale check only."
fi

printf 'ts_hostname=%s\nts_ip=%s\nsshd_port=2222\nssh_user=%s\nts_home=%s\nboot_time=%s\n' \
  "$TS_HOSTNAME" "$TSIP" "$(id -un)" "$TS_HOME" "$(date -Is)" \
  | tee "$WS/.hidevlab_tailscale.env" | tee -a "$LOG"
# PATH 提示
echo "export PATH=$TS_HOME/bin:\$PATH" > "$WS/hidevlab_tailscale_path.sh"
echo "alias ts='$TS_HOME/bin/tailscale --socket=$SOCK'" >> "$WS/hidevlab_tailscale_path.sh"

log "===== hidevlab bootstrap done (static) ====="
"$TS_HOME/bin/tailscale" --socket="$SOCK" status | head -20 | tee -a "$LOG" || true
exit 0
