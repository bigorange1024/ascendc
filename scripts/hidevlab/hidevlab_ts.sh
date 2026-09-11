#!/usr/bin/env bash
# HiDevLab Tailscale 一键入网（人在 WebIDE 终端跑；也可被 webide_start.sh 调用）。
# 权威说明：docs/engineering/HiDevLab-Agent连接.md
#
#   TS_AUTHKEY='tskey-…' bash scripts/hidevlab/hidevlab_ts.sh
#
# 必须 userspace：容器无 CAP_NET_ADMIN / 无可用 TUN。
# 硬禁：apt 装 tailscale、内核态 TUN、tailscale up --ssh。
#
# 网络差时易「假死」点（已全部加超时）：
#   - curl 下静态包（默认 90s；可预放包跳过下载）
#   - tailscale up 连控制面（默认 60s）
#   - serve 声明（默认 20s）
set -u

if [ -z "${TS_AUTHKEY:-}" ] && [ -n "${TAILSCALE_AUTHKEY:-}" ]; then
  TS_AUTHKEY="$TAILSCALE_AUTHKEY"
fi
: "${TS_AUTHKEY:?请设置 TS_AUTHKEY（或 TAILSCALE_AUTHKEY）}"

TS_HOSTNAME="${TS_HOSTNAME:-hidevlab-npu}"
PUB="${CURSOR_PUBKEY:-ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIB4+JMqBJWCNaAyd3fvpTHZ371OhctRYObuG2GUxDDof cursor-agent}"
WS="${WORKSPACE_ROOT:-/workspace}"
PERSIST="${HIDEVLAB_PERSIST_DIR:-$WS/user_data}"
TS_HOME="${TS_HOME:-$PERSIST/tailscale}"
SOCK="$TS_HOME/run/tailscaled.sock"
LOG="${HIDEVLAB_TS_LOG:-$PERSIST/hidevlab_ts.log}"
ENV_OUT="${HIDEVLAB_TS_ENV:-$PERSIST/hidevlab_tailscale.env}"
CURL_MAX="${TS_CURL_MAX_SEC:-90}"
UP_MAX="${TS_UP_MAX_SEC:-60}"
SERVE_MAX="${TS_SERVE_MAX_SEC:-20}"

mkdir -p "$TS_HOME/bin" "$TS_HOME/state" "$TS_HOME/run" "$WS" "$PERSIST"
# 让 tee 尽快刷出，避免「没输出像死了」
export PYTHONUNBUFFERED=1
log() { echo "[$(date '+%F %T')] $*" | tee -a "$LOG"; }

run_timeout() {
  # run_timeout <sec> <cmd...>  —— 无 timeout 命令时用后台+wait 兜底
  local sec="$1"; shift
  if command -v timeout >/dev/null 2>&1; then
    timeout --foreground -k 5 "$sec" "$@"
    return $?
  fi
  "$@" &
  local pid=$!
  local i=0
  while kill -0 "$pid" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -ge "$sec" ]; then
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
      return 124
    fi
    sleep 1
  done
  wait "$pid"
}

arch="$(uname -m)"
case "$arch" in
  aarch64|arm64) TS_ARCH=arm64 ;;
  x86_64|amd64)  TS_ARCH=amd64 ;;
  *) log "!! unsupported arch=$arch"; exit 1 ;;
esac

log "===== hidevlab_ts begin ====="
log "host=$(hostname) arch=$TS_ARCH TS_HOME=$TS_HOME hostname=$TS_HOSTNAME"

# ---------- 1) 静态包 ----------
# 优先顺序：已有 bin → 预放的 ts.tgz / TS_TGZ → 联网下载（带超时）
need_bin=0
if [ ! -x "$TS_HOME/bin/tailscale" ] || [ ! -x "$TS_HOME/bin/tailscaled" ]; then
  need_bin=1
fi

install_from_tgz() {
  local tg="$1"
  log "extract $tg ..."
  rm -rf "$TS_HOME/extract"; mkdir -p "$TS_HOME/extract"
  tar -xzf "$tg" -C "$TS_HOME/extract" || return 1
  local src
  src="$(find "$TS_HOME/extract" -maxdepth 1 -type d -name 'tailscale_*' | head -1)"
  [ -n "$src" ] || return 1
  cp -f "$src/tailscale" "$src/tailscaled" "$TS_HOME/bin/"
  chmod +x "$TS_HOME/bin/tailscale" "$TS_HOME/bin/tailscaled"
  log "static ok: $($TS_HOME/bin/tailscale version 2>/dev/null | head -1)"
}

if [ "$need_bin" = 1 ]; then
  log "step1: need static binaries"
  tg="$TS_HOME/ts.tgz"
  ok=0
  # 预放包：环境变量或持久盘常见路径
  for cand in "${TS_TGZ:-}" "$PERSIST/tailscale_latest_${TS_ARCH}.tgz" "$PERSIST/ts.tgz" "$tg"; do
    [ -n "$cand" ] && [ -f "$cand" ] || continue
    log "try local tarball: $cand"
    if install_from_tgz "$cand"; then ok=1; break; fi
  done
  if [ "$ok" != 1 ]; then
    log "step1: download (timeout ${CURL_MAX}s) — HiDevLab 若出网差会在这里失败而不是死等"
    url1="https://pkgs.tailscale.com/stable/tailscale_latest_${TS_ARCH}.tgz"
    # 备用：GitHub latest 资产名不固定，先试 pkgs；失败给出手工落盘指引
    if ! run_timeout "$CURL_MAX" curl -fL --connect-timeout 10 --retry 2 --retry-delay 2 \
        -o "$tg" "$url1"; then
      log "!! 下载失败/超时（rc=$?）。本机出网往往到不了 pkgs.tailscale.com。"
      log "   手工解决：在能上网的机器下载后拷进盒子，再重跑："
      log "     # 下载：curl -fL -o tailscale_latest_${TS_ARCH}.tgz $url1"
      log "     # 拷到：/workspace/user_data/tailscale_latest_${TS_ARCH}.tgz"
      log "     # 或：  TS_TGZ=/path/to/ts.tgz TS_AUTHKEY=… bash …/hidevlab_ts.sh"
      exit 10
    fi
    install_from_tgz "$tg" || { log "!! extract failed"; exit 11; }
  fi
else
  log "step1: binaries present, skip download ($($TS_HOME/bin/tailscale version 2>/dev/null | head -1))"
fi

export PATH="$TS_HOME/bin:$PATH"
TS="$TS_HOME/bin/tailscale"
TSD="$TS_HOME/bin/tailscaled"

# ---------- 2) userspace daemon ----------
log "step2: ensure tailscaled (userspace)"
if ! pgrep -f "$TS_HOME/bin/tailscaled" >/dev/null 2>&1; then
  nohup "$TSD" \
    --tun=userspace-networking \
    --state="$TS_HOME/state/tailscaled.state" \
    --socket="$SOCK" \
    --socks5-server=localhost:1055 \
    >"$TS_HOME/tailscaled.log" 2>&1 &
  # 等 sock 出现，最多 15s
  for i in $(seq 1 15); do
    [ -S "$SOCK" ] && break
    sleep 1
  done
else
  log "tailscaled already running"
fi
if ! pgrep -f "$TS_HOME/bin/tailscaled" >/dev/null 2>&1; then
  log "!! tailscaled failed; dump log"
  tail -40 "$TS_HOME/tailscaled.log" 2>/dev/null | tee -a "$LOG" || true
  exit 12
fi
log "tailscaled ok (sock=$SOCK)"

# ---------- 3) up（带超时）----------
log "step3: tailscale up (timeout ${UP_MAX}s) hostname=$TS_HOSTNAME"
if ! run_timeout "$UP_MAX" "$TS" --socket="$SOCK" up \
  --authkey="$TS_AUTHKEY" --hostname="$TS_HOSTNAME" \
  --accept-dns=false --accept-routes=false; then
  rc=$?
  log "!! tailscale up 失败/超时 rc=$rc"
  log "   常见：AUTHKEY 无效/一次性已用尽；或出网到 Tailscale 控制面被拦"
  "$TS" --socket="$SOCK" status 2>&1 | head -20 | tee -a "$LOG" || true
  tail -30 "$TS_HOME/tailscaled.log" 2>/dev/null | tee -a "$LOG" || true
  exit 13
fi
TSIP="$("$TS" --socket="$SOCK" ip -4 2>/dev/null | head -1)"
[ -n "$TSIP" ] || { log "!! no ipv4 after up"; exit 14; }
log "ts_ip=$TSIP"

# ---------- 4) authorized_keys ----------
log "step4: authorize cursor pubkey"
mkdir -p ~/.ssh && chmod 700 ~/.ssh
touch ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys
grep -qF "$PUB" ~/.ssh/authorized_keys 2>/dev/null || echo "$PUB" >> ~/.ssh/authorized_keys

# ---------- 5) loopback sshd ----------
log "step5: sshd 127.0.0.1:2222"
SSHD="$(command -v sshd || true)"; [ -x "${SSHD:-}" ] || SSHD=/usr/sbin/sshd
if [ ! -x "$SSHD" ]; then
  log "!! 无 sshd。请换带 openssh-server 的镜像。"
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
log "sshd ok"

# ---------- 6) serve（带超时）----------
log "step6: serve tcp:2222 (timeout ${SERVE_MAX}s)"
run_timeout "$SERVE_MAX" "$TS" --socket="$SOCK" serve --bg --tcp 2222 tcp://127.0.0.1:2222 >/dev/null 2>&1 \
  || run_timeout "$SERVE_MAX" "$TS" --socket="$SOCK" serve --bg --tcp=2222 tcp://127.0.0.1:2222 >/dev/null 2>&1 \
  || log "WARN: serve 声明超时/失败（netstack 默认仍可能转发；可稍后手动 serve）"
log "serve step done"

# ---------- 7) 摘要 ----------
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
run_timeout 10 "$TS" --socket="$SOCK" status 2>/dev/null | head -15 | tee -a "$LOG" || true
echo
echo "把 $ENV_OUT 内容贴回 Agent 即可（不要贴 TS_AUTHKEY）。"
