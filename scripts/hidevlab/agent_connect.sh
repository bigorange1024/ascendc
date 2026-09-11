#!/usr/bin/env bash
# Cursor Cloud Agent 侧 —— 连上已启动的 HiDevLab WebIDE（经 Tailscale）。
#
# 前提：人在 WebIDE 已跑过 webide_start.sh（tailnet 节点在线、2222 serve 好）。
#
# 用法（Agent / 本机）：
#   bash scripts/hidevlab/agent_connect.sh              # 探活并打印 SSH 命令
#   bash scripts/hidevlab/agent_connect.sh --exec 'npu-smi info | head'
#   eval "$(bash scripts/hidevlab/agent_connect.sh --export)"   # 导出 HIDEVLAB_SSH / HIDEVLAB_HOST
#
# 依赖 Cloud Secret：
#   TAILSCALE_AUTHKEY（或 TS_AUTHKEY）
#   CANNLAB_SSH_KEY —— 格式坏了也没关系，本脚本会自动 materialize
#
# 与 CANNLab 路径分开；hostname 默认 hidevlab-npu。
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE=probe
REMOTE_CMD=""
while [ $# -gt 0 ]; do
  case "$1" in
    --exec) MODE=exec; REMOTE_CMD="${2:-}"; shift 2 || true ;;
    --export) MODE=export; shift ;;
    --probe) MODE=probe; shift ;;
    -h|--help)
      sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

HOST="${HIDEVLAB_HOST:-${TS_HOSTNAME:-hidevlab-npu}}"
PORT="${HIDEVLAB_SSH_PORT:-2222}"
KEY_OUT="${HIDEVLAB_KEY_FILE:-$HOME/.ssh/hidevlab}"
TS_SOCK="${TS_SOCK:-/var/run/tailscale/tailscaled.sock}"
TS_STATE="${TS_STATE:-/var/lib/tailscale/tailscaled.state}"
SOCKS="${HIDEVLAB_SOCKS:-127.0.0.1:1055}"
AGENT_HOSTNAME="${CURSOR_TS_HOSTNAME:-cursor-agent-$(hostname -s 2>/dev/null || echo box)}"

# ----- authkey -----
if [ -z "${TS_AUTHKEY:-}" ]; then
  if [ -n "${TAILSCALE_AUTHKEY:-}" ]; then TS_AUTHKEY="$TAILSCALE_AUTHKEY"
  fi
fi
: "${TS_AUTHKEY:?需要 TS_AUTHKEY 或 TAILSCALE_AUTHKEY}"

# ----- 1) 私钥兜底重建 -----
bash "$SCRIPT_DIR/materialize_ssh_key.sh" "$KEY_OUT" >/tmp/materialize_hidevlab.log 2>&1 || {
  echo "!! materialize 失败：" >&2
  cat /tmp/materialize_hidevlab.log >&2
  exit 3
}
# 只回显校验行
grep -E 'materialize|ssh-ed25519|校验' /tmp/materialize_hidevlab.log || true

# ----- 2) 本机 userspace tailscaled -----
need_start=0
if ! pgrep -x tailscaled >/dev/null 2>&1; then need_start=1; fi
if [ ! -S "$TS_SOCK" ]; then need_start=1; fi
if [ "$need_start" = 1 ]; then
  echo "[agent] start local userspace tailscaled ..."
  if command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
    sudo mkdir -p "$(dirname "$TS_SOCK")" "$(dirname "$TS_STATE")"
    sudo setsid nohup tailscaled \
      --tun=userspace-networking \
      --socks5-server="$SOCKS" \
      --state="$TS_STATE" \
      --socket="$TS_SOCK" \
      </dev/null >/tmp/tailscaled-agent.log 2>&1 &
  else
    mkdir -p "$HOME/.tailscale-agent/run" "$HOME/.tailscale-agent/state"
    TS_SOCK="$HOME/.tailscale-agent/run/tailscaled.sock"
    TS_STATE="$HOME/.tailscale-agent/state/tailscaled.state"
    setsid nohup tailscaled \
      --tun=userspace-networking \
      --socks5-server="$SOCKS" \
      --state="$TS_STATE" \
      --socket="$TS_SOCK" \
      </dev/null >/tmp/tailscaled-agent.log 2>&1 &
  fi
  sleep 3
fi

ts() {
  if [ -S "$TS_SOCK" ]; then
    if command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
      sudo tailscale --socket="$TS_SOCK" "$@"
    else
      tailscale --socket="$TS_SOCK" "$@"
    fi
  else
    tailscale "$@"
  fi
}

# 已在网则跳过；否则 up
if ! ts status --self 2>/dev/null | grep -q '100\.'; then
  echo "[agent] tailscale up hostname=$AGENT_HOSTNAME ..."
  ts up --authkey="$TS_AUTHKEY" --hostname="$AGENT_HOSTNAME" --accept-dns=false || \
    ts up --reset --authkey="$TS_AUTHKEY" --hostname="$AGENT_HOSTNAME" --accept-dns=false
fi

SSH_BASE=(
  ssh
  -o "ProxyCommand=nc -X 5 -x ${SOCKS} %h %p"
  -o StrictHostKeyChecking=accept-new
  -o IdentitiesOnly=yes
  -i "$KEY_OUT"
  -o ConnectTimeout=25
  -o ConnectionAttempts=3
  -o ServerAliveInterval=15
  -p "$PORT"
  "root@${HOST}"
)

case "$MODE" in
  export)
    # 供 eval：注意单引号保护
    printf "export HIDEVLAB_HOST=%q\n" "$HOST"
    printf "export HIDEVLAB_KEY_FILE=%q\n" "$KEY_OUT"
    printf "export HIDEVLAB_SOCKS=%q\n" "$SOCKS"
    printf "HIDEVLAB_SSH=(ssh -o ProxyCommand=\"nc -X 5 -x %s %%h %%p\" -o StrictHostKeyChecking=accept-new -o IdentitiesOnly=yes -i %q -o ConnectTimeout=25 -o ConnectionAttempts=3 -o ServerAliveInterval=15 -p %q root@%q)\n" \
      "$SOCKS" "$KEY_OUT" "$PORT" "$HOST"
    echo "# 用法: \"\${HIDEVLAB_SSH[@]}\" 'echo OK'"
    ;;
  exec)
    [ -n "$REMOTE_CMD" ] || { echo "!! --exec 需要命令" >&2; exit 2; }
    ok=0
    for a in 1 2 3 4 5; do
      if "${SSH_BASE[@]}" "$REMOTE_CMD"; then ok=1; break; fi
      echo "[agent] ssh retry $a ..."; sleep 4
    done
    [ "$ok" = 1 ] || exit 5
    ;;
  probe|*)
    echo "[agent] probe root@${HOST}:${PORT} via socks ${SOCKS} ..."
    ok=0
    for a in 1 2 3 4 5; do
      if out="$("${SSH_BASE[@]}" 'echo CONNECT_OK; hostname; date -Is' 2>/tmp/hidevlab_ssh.err)"; then
        echo "$out"
        ok=1
        break
      fi
      echo "[agent] ssh retry $a ..."; sleep 4
    done
    if [ "$ok" != 1 ]; then
      echo "!! 连不上 ${HOST}:${PORT}" >&2
      echo "   常见原因：WebIDE 未跑 webide_start.sh / sshd 掉了 / 节点离线" >&2
      echo "   本机 tailscale status:" >&2
      ts status 2>&1 | head -15 >&2 || true
      tail -5 /tmp/hidevlab_ssh.err 2>/dev/null >&2 || true
      exit 5
    fi
    echo
    echo "可用："
    echo "  bash $SCRIPT_DIR/agent_connect.sh --exec 'npu-smi info | head'"
    echo "  eval \"\$(bash $SCRIPT_DIR/agent_connect.sh --export)\""
    ;;
esac
