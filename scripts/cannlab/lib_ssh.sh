#!/usr/bin/env bash
# CANNLab 短连 SSH 公共库：SOCKS + ServerAlive + 失败时重试 / Tailscale reset
# 被 remote_job.sh / run_npu_ab_nohup.sh / agent_link_keepalive.sh / which_npu.sh source。
#
# 重要：云主机每次启动都是新节点（IP / MagicDNS 会变：cannlab-npu、cannlab-npu-1…）。
# 默认 **自动挑 online 的 cannlab-npu***，不要写死主机名。仅 SSH_HOST_FORCE 可强制。
# shellcheck disable=SC2034

SSH_KEY="${SSH_KEY:-$HOME/.ssh/cannlab}"
SSH_PORT="${SSH_PORT:-2222}"
SOCKS="${SOCKS:-127.0.0.1:1055}"
TS_SOCK="${TS_SOCK:-/var/run/tailscale/tailscaled.sock}"
TS_HOSTNAME="${TS_HOSTNAME:-cursor-agent}"
# SSH_HOST 由 cannlab_pick_host 填充；调用前可不设

cannlab_ssh_base() {
  local -n _out="$1"
  : "${SSH_HOST:?SSH_HOST unset — call cannlab_pick_host first}"
  _out=(ssh
    -o ServerAliveInterval=15
    -o ServerAliveCountMax=4
    -o ConnectTimeout=25
    -o ProxyCommand="nc -X 5 -x ${SOCKS} %h %p"
    -o StrictHostKeyChecking=accept-new
    -i "${SSH_KEY}"
    -p "${SSH_PORT}"
    "developer@${SSH_HOST}"
  )
}

cannlab_scp_opts() {
  local -n _out="$1"
  _out=(-o ProxyCommand="nc -X 5 -x ${SOCKS} %h %p"
    -o StrictHostKeyChecking=accept-new
    -o ConnectTimeout=25
    -i "${SSH_KEY}"
    -P "${SSH_PORT}"
  )
}

cannlab_ensure_tailscale() {
  if ! command -v tailscale >/dev/null 2>&1; then
    return 0
  fi
  if ! tailscale --socket="${TS_SOCK}" status >/dev/null 2>&1; then
    if [ -n "${TAILSCALE_AUTHKEY:-}" ]; then
      echo "[cannlab-ssh] tailscale status fail → up --reset" >&2
      sudo tailscale --socket="${TS_SOCK}" up --reset \
        --authkey="${TAILSCALE_AUTHKEY}" --hostname="${TS_HOSTNAME}" >/dev/null 2>&1 || true
      sleep 2
    fi
    return 0
  fi
  if tailscale --socket="${TS_SOCK}" status 2>/dev/null | grep -E "^\S+\s+${TS_HOSTNAME}\b" | grep -q offline; then
    if [ -n "${TAILSCALE_AUTHKEY:-}" ]; then
      echo "[cannlab-ssh] self offline → up --reset" >&2
      sudo tailscale --socket="${TS_SOCK}" up --reset \
        --authkey="${TAILSCALE_AUTHKEY}" --hostname="${TS_HOSTNAME}" >/dev/null 2>&1 || true
      sleep 2
    fi
  fi
}

# 在 status 里找 online 的 cannlab-npu*（优先精确 cannlab-npu，再 -N）
cannlab_pick_host() {
  if [ -n "${SSH_HOST_FORCE:-}" ]; then
    SSH_HOST="${SSH_HOST_FORCE}"
    echo "[cannlab-ssh] SSH_HOST_FORCE=${SSH_HOST}" >&2
    return 0
  fi

  cannlab_ensure_tailscale
  local st line host ip best="" best_ip=""
  st="$(tailscale --socket="${TS_SOCK}" status 2>/dev/null || true)"

  # 第一遍：精确名 cannlab-npu 且 online
  while IFS= read -r line; do
    echo "${line}" | grep -qi offline && continue
    host="$(echo "${line}" | awk '{print $2}')"
    ip="$(echo "${line}" | awk '{print $1}')"
    if [ "${host}" = "cannlab-npu" ]; then
      SSH_HOST="${host}"
      echo "[cannlab-ssh] pick ${SSH_HOST} (${ip})" >&2
      return 0
    fi
  done <<< "${st}"

  # 第二遍：任意 cannlab-npu-* online（取第一个；多台时请 SSH_HOST_FORCE）
  while IFS= read -r line; do
    echo "${line}" | grep -qi offline && continue
    host="$(echo "${line}" | awk '{print $2}')"
    ip="$(echo "${line}" | awk '{print $1}')"
    case "${host}" in
      cannlab-npu-*)
        SSH_HOST="${host}"
        echo "[cannlab-ssh] pick ${SSH_HOST} (${ip})  # 精确名被旧 offline 节点占着" >&2
        return 0
        ;;
    esac
  done <<< "${st}"

  SSH_HOST="cannlab-npu"
  echo "[cannlab-ssh] WARN: no online cannlab-npu*; fallback ${SSH_HOST}（请确认已 bootstrap）" >&2
  return 1
}

cannlab_ssh_try() {
  local n=1 delay=4
  local sshc=()
  # 每次尝试前重 pick（节点可能刚上线或换名）
  cannlab_pick_host || true
  cannlab_ssh_base sshc
  while [ "$n" -le 8 ]; do
    cannlab_ensure_tailscale
    cannlab_pick_host || true
    cannlab_ssh_base sshc
    if "${sshc[@]}" "$@"; then
      return 0
    fi
    echo "[cannlab-ssh] fail attempt ${n} host=${SSH_HOST:-?} ; sleep ${delay}s" >&2
    if [ -n "${TAILSCALE_AUTHKEY:-}" ] && [ $((n % 2)) -eq 0 ]; then
      sudo tailscale --socket="${TS_SOCK}" up --reset \
        --authkey="${TAILSCALE_AUTHKEY}" --hostname="${TS_HOSTNAME}" >/dev/null 2>&1 || true
      sleep 2
    fi
    sleep "${delay}"
    delay=$((delay * 2))
    [ "${delay}" -gt 60 ] && delay=60
    n=$((n + 1))
  done
  return 1
}
