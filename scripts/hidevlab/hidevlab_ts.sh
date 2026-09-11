#!/usr/bin/env bash
# HiDevLab Tailscale 一键入网（人在 WebIDE 终端只跑这一条）。
# 权威说明：docs/engineering/HiDevLab-WebIDE操作手册.md（§Tailscale 驱动 Agent 访问）。
# 与 GitCode CANNLab（scripts/cannlab/agent_bootstrap.sh）**分开维护**——手册 §10 禁止两路混用。
#
#   TS_AUTHKEY='tskey-auth-…' bash scripts/hidevlab/hidevlab_ts.sh
#
# 为什么必须 userspace：HiDevLab 容器**无 CAP_NET_ADMIN、无可用 /dev/net/tun**
#   （2026-09-11 实测 CapBnd=0xa80425fb 无 bit12），内核态 tailscaled 会
#   `tstun.New("tailscale0"): operation not permitted` 直接崩。故只能
#   `--tun=userspace-networking`：由 netstack 把 tailnet 入站端口转发到 127.0.0.1，
#   本地 sshd 因此**只需听 127.0.0.1:2222**（切勿绑 tailscale IP——用户态无网卡持有它）。
#
# 硬禁（会崩 WebIDE / 无权限）：apt 装 tailscale、内核态 tailscaled、tailscale up --ssh。
#
# 持久性：/workspace 根在 overlay（重启可能丢），持久可写盘为 /workspace/user_data。
#   如需重启后可复跑，把本脚本与静态包一起放到 /workspace/user_data/ 再从那里跑。
set -u
# 兼容 Cloud Secret 名 TAILSCALE_AUTHKEY
if [ -z "${TS_AUTHKEY:-}" ] && [ -n "${TAILSCALE_AUTHKEY:-}" ]; then
  TS_AUTHKEY="$TAILSCALE_AUTHKEY"
fi
: "${TS_AUTHKEY:?请设置 TS_AUTHKEY（或 TAILSCALE_AUTHKEY；不要贴到聊天；只在本机终端导出；须 reusable）}"

TS_HOSTNAME="${TS_HOSTNAME:-hidevlab-npu}"   # 勿用 cannlab-npu（那是另一套环境，见手册 §7）
# 默认授权公钥 = Cursor Agent 密钥对公钥（私钥 Secret CANNLAB_SSH_KEY；格式坏了用 materialize_ssh_key.sh 兜底）。
PUB="${CURSOR_PUBKEY:-ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIB4+JMqBJWCNaAyd3fvpTHZ371OhctRYObuG2GUxDDof cursor-agent}"
WS="${WORKSPACE_ROOT:-/workspace}"
# 默认落持久盘：/workspace 根在 overlay，重启会丢；user_data 才持久。
PERSIST="${HIDEVLAB_PERSIST_DIR:-$WS/user_data}"
TS_HOME="${TS_HOME:-$PERSIST/tailscale}"
SOCK="$TS_HOME/run/tailscaled.sock"
LOG="${HIDEVLAB_TS_LOG:-$PERSIST/hidevlab_ts.log}"
ENV_OUT="${HIDEVLAB_TS_ENV:-$PERSIST/hidevlab_tailscale.env}"
mkdir -p "$TS_HOME/bin" "$TS_HOME/state" "$TS_HOME/run" "$WS" "$PERSIST"

log() { echo "[$(date '+%F %T')] $*" | tee -a "$LOG"; }

arch="$(uname -m)"
case "$arch" in
  aarch64|arm64) TS_ARCH=arm64 ;;
  x86_64|amd64)  TS_ARCH=amd64 ;;
  *) log "!! unsupported arch=$arch"; exit 1 ;;
esac

# ---------- 1) 静态包（仅缺失时下载；不用 apt，避免污染镜像）----------
if [ ! -x "$TS_HOME/bin/tailscale" ] || [ ! -x "$TS_HOME/bin/tailscaled" ]; then
  log "FIRST: download static tailscale ($TS_ARCH)"
  tg="$TS_HOME/ts.tgz"
  curl -fL "https://pkgs.tailscale.com/stable/tailscale_latest_${TS_ARCH}.tgz" -o "$tg"
  rm -rf "$TS_HOME/extract"; mkdir -p "$TS_HOME/extract"
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

# ---------- 2) userspace daemon（本镜像无 /dev/net/tun / 无 CAP_NET_ADMIN）----------
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

# ---------- 3) 入网（禁 --ssh：内核 SSH 服务在无 TUN 镜像会崩）----------
log "tailscale up hostname=$TS_HOSTNAME"
"$TS" --socket="$SOCK" up \
  --authkey="$TS_AUTHKEY" --hostname="$TS_HOSTNAME" \
  --accept-dns=false --accept-routes=false
TSIP="$("$TS" --socket="$SOCK" ip -4 | head -1)"
[ -n "$TSIP" ] || { log "!! no ipv4"; "$TS" --socket="$SOCK" status 2>&1 | tee -a "$LOG" || true; exit 1; }
log "ts_ip=$TSIP"

# ---------- 4) 授权 Cursor 公钥 ----------
mkdir -p ~/.ssh && chmod 700 ~/.ssh
touch ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys
grep -qF "$PUB" ~/.ssh/authorized_keys 2>/dev/null || echo "$PUB" >> ~/.ssh/authorized_keys

# ---------- 5) 本机 sshd :2222（只听 127.0.0.1，不碰 22）----------
# 用户态下 netstack 把 tailnet:2222 转到 127.0.0.1:2222，故绑 loopback 即可（绑 TSIP 会永远起不来）。
SSHD="$(command -v sshd || true)"; [ -x "${SSHD:-}" ] || SSHD=/usr/sbin/sshd
if [ ! -x "$SSHD" ]; then
  log "!! 无 sshd，无法给 Agent 开壳（本镜像勿 apt 乱装）。请换带 openssh-server 的镜像或手工装好后再跑。"
  exit 2
fi
mkdir -p /run/sshd; ssh-keygen -A 2>/dev/null || true
if [ -f /tmp/sshd2222.pid ]; then
  op=$(cat /tmp/sshd2222.pid 2>/dev/null || true)
  [ -n "$op" ] && kill -0 "$op" 2>/dev/null && { kill "$op" 2>/dev/null || true; sleep 1; }
fi
"$SSHD" -p 2222 -o ListenAddress=127.0.0.1 \
  -o PermitRootLogin=prohibit-password -o PasswordAuthentication=no \
  -o PubkeyAuthentication=yes -o PidFile=/tmp/sshd2222.pid
log "sshd 127.0.0.1:2222 ok"

# ---------- 6) serve：显式把 tailnet:2222 → 127.0.0.1:2222 ----------
# netstack 默认也会转发到 loopback；这里再用 serve 显式声明，路由更稳、便于 `serve status` 排障。
"$TS" --socket="$SOCK" serve --bg --tcp 2222 tcp://127.0.0.1:2222 >/dev/null 2>&1 || \
  "$TS" --socket="$SOCK" serve --bg --tcp=2222 tcp://127.0.0.1:2222
log "serve tcp:2222 ok"

# ---------- 7) 摘要（贴回 Agent；勿贴 TS_AUTHKEY）----------
{
  echo "ts_hostname=$TS_HOSTNAME"
  echo "ts_ip=$TSIP"
  echo "sshd_port=2222"
  echo "ssh_user=$(id -un)"
  echo "mode=userspace+serve"
  echo "ts_home=$TS_HOME"
  echo "boot_time=$(date -Is)"
} | tee "$ENV_OUT" | tee "$WS/.hidevlab_tailscale.env" | tee -a "$LOG"

log "===== hidevlab_ts done ====="
"$TS" --socket="$SOCK" status | head -15 | tee -a "$LOG" || true
"$TS" --socket="$SOCK" serve status 2>&1 | head -12 | tee -a "$LOG" || true
echo
echo "把 $ENV_OUT 内容贴回 Agent 即可（不要贴 TS_AUTHKEY）。"
