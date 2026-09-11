#!/usr/bin/env bash
# HiDevLab Tailscale SSH 库（Cursor Agent 侧）。
# 依赖：本机 userspace tailscaled SOCKS5 127.0.0.1:1055；私钥 ~/.ssh/cannlab。
# 用法：
#   source scripts/hidevlab/lib_ssh.sh
#   hidevlab_ensure_agent_tailscale
#   hidevlab_pick_host
#   hidevlab_ssh 'echo CONNECTED'
set -u

HIDEVLAB_SOCKS="${HIDEVLAB_SOCKS:-127.0.0.1:1055}"
HIDEVLAB_SSH_KEY="${HIDEVLAB_SSH_KEY:-${HOME}/.ssh/cannlab}"
HIDEVLAB_SSH_PORT="${HIDEVLAB_SSH_PORT:-2222}"
HIDEVLAB_SSH_USER="${HIDEVLAB_SSH_USER:-root}"
HIDEVLAB_HOST_PREFIX="${HIDEVLAB_HOST_PREFIX:-hidevlab-npu}"

hidevlab_ensure_agent_tailscale() {
  command -v nc >/dev/null 2>&1 || sudo apt-get install -y -qq netcat-openbsd >/dev/null
  if ! pgrep -f 'tailscaled.*userspace-networking' >/dev/null 2>&1; then
    command -v tailscale >/dev/null 2>&1 || curl -fsSL https://tailscale.com/install.sh | sudo bash
    sudo mkdir -p /var/lib/tailscale /var/run/tailscale
    sudo tailscaled --tun=userspace-networking \
      --socks5-server=localhost:1055 \
      --outbound-http-proxy-listen=localhost:1080 \
      --state=/var/lib/tailscale/tailscaled.state \
      --socket=/var/run/tailscale/tailscaled.sock >/tmp/tailscaled.log 2>&1 &
    sleep 3
  fi
  if [ -n "${TAILSCALE_AUTHKEY:-}" ]; then
    if ! sudo tailscale --socket=/var/run/tailscale/tailscaled.sock status >/dev/null 2>&1; then
      sudo tailscale --socket=/var/run/tailscale/tailscaled.sock up \
        --authkey="${TAILSCALE_AUTHKEY}" --hostname=cursor-agent
    fi
    # peer 全 timeout 时重置一次（Cloud Agent 长闲常见）
    if ! sudo tailscale --socket=/var/run/tailscale/tailscaled.sock ping -c 1 -t 2 "${HIDEVLAB_HOST_PREFIX}" >/dev/null 2>&1; then
      sudo tailscale --socket=/var/run/tailscale/tailscaled.sock up --reset \
        --authkey="${TAILSCALE_AUTHKEY}" --hostname=cursor-agent >/dev/null 2>&1 || true
    fi
  fi
  if [ ! -f "${HIDEVLAB_SSH_KEY}" ] && [ -n "${CANNLAB_SSH_KEY:-}" ]; then
    mkdir -p "${HOME}/.ssh" && chmod 700 "${HOME}/.ssh"
    printf '%s\n' "${CANNLAB_SSH_KEY}" > "${HIDEVLAB_SSH_KEY}"
    chmod 600 "${HIDEVLAB_SSH_KEY}"
  fi
}

# 在线节点里挑 hidevlab-npu*（优先无后缀）。同时导出 SSH_HOST_IP（SOCKS 下 IP 比 MagicDNS 稳）。
hidevlab_pick_host() {
  local sock=/var/run/tailscale/tailscaled.sock
  local ip host rest best="" best_ip=""
  # 先轻量 ping 唤醒 DERP（userspace↔userspace 常无直连）
  sudo tailscale --socket="${sock}" ping -c 1 -t 3 "${HIDEVLAB_HOST_PREFIX}" >/dev/null 2>&1 || true
  while read -r ip host rest; do
    case "${host}" in
      ${HIDEVLAB_HOST_PREFIX}|${HIDEVLAB_HOST_PREFIX}-*)
        if echo "${rest}" | grep -vq offline; then
          if [ -z "${best}" ] || [ "${#host}" -lt "${#best}" ]; then
            best="${host}"
            best_ip="${ip}"
          fi
        fi
        ;;
    esac
  done < <(sudo tailscale --socket="${sock}" status 2>/dev/null | awk 'NR>0 && $1 ~ /^[0-9]+\./ {print $1, $2, substr($0, index($0,$3))}')
  if [ -z "${best}" ]; then
    echo "!! 无在线 ${HIDEVLAB_HOST_PREFIX}*；请先在 WebIDE 完成静态包+userspace+serve" >&2
    sudo tailscale --socket="${sock}" status 2>&1 | head -30 >&2 || true
    return 1
  fi
  SSH_HOST="${best}"
  SSH_HOST_IP="${best_ip}"
  export SSH_HOST SSH_HOST_IP
  echo "${SSH_HOST}"
}

hidevlab_ssh() {
  local target="${SSH_HOST_IP:-${SSH_HOST:?先 hidevlab_pick_host}}"
  touch /tmp/.hidevlab_agent_touch 2>/dev/null || true
  ssh -o ProxyCommand="nc -X 5 -x ${HIDEVLAB_SOCKS} %h %p" \
    -o StrictHostKeyChecking=accept-new \
    -o ServerAliveInterval=15 \
    -o BatchMode=yes \
    -o IdentitiesOnly=yes \
    -i "${HIDEVLAB_SSH_KEY}" \
    -p "${HIDEVLAB_SSH_PORT}" \
    "${HIDEVLAB_SSH_USER}@${target}" "$@"
}

# 远程执行并刷新心跳
hidevlab_ssh_try() {
  hidevlab_ssh bash -lc "touch /workspace/.agent_heartbeat 2>/dev/null; $*"
}
