#!/usr/bin/env bash
# agent_link_keepalive.sh — 本机双端保活，防空闲断链
#
# 每轮：自动 pick online cannlab-npu*（主机每次启动会换名/IP）→ 短 SSH touch 心跳
# + 戳本机 Tailscale（失败则 up --reset）
#
#   nohup bash scripts/cannlab/agent_link_keepalive.sh >/tmp/cannlab_keepalive.log 2>&1 &
# 一般不必设 SSH_HOST；强制某台：SSH_HOST_FORCE=cannlab-npu-2
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib_ssh.sh
source "${SCRIPT_DIR}/lib_ssh.sh"

INTERVAL="${KEEPALIVE_INTERVAL:-40}"
LOG="${KEEPALIVE_LOG:-/tmp/cannlab_keepalive.log}"

echo "[keepalive] start interval=${INTERVAL}s (auto-pick each loop) $(date -Iseconds)" | tee -a "${LOG}"

fail=0
while true; do
  cannlab_ensure_tailscale
  if ! cannlab_pick_host; then
    fail=$((fail + 1))
    echo "[keepalive] no online host streak=${fail} $(date -Iseconds)" | tee -a "${LOG}"
    sleep "${INTERVAL}"
    continue
  fi
  if ! nc -z -w 3 127.0.0.1 1055 2>/dev/null; then
    echo "[keepalive] SOCKS 1055 down $(date -Iseconds)" | tee -a "${LOG}"
    fail=$((fail + 1))
  fi
  if cannlab_ssh_try 'touch /mnt/workspace/.agent_heartbeat && echo OK $(date -Iseconds)'; then
    fail=0
  else
    fail=$((fail + 1))
    echo "[keepalive] ssh fail streak=${fail} last_host=${SSH_HOST:-none} $(date -Iseconds)" | tee -a "${LOG}"
  fi
  sleep "${INTERVAL}"
done
