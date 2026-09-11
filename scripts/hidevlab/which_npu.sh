#!/usr/bin/env bash
# 打印当前在线的 HiDevLab Tailscale 主机名。
#   bash scripts/hidevlab/which_npu.sh
#   bash scripts/hidevlab/which_npu.sh --export   # → export SSH_HOST=...
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib_ssh.sh
source "${SCRIPT_DIR}/lib_ssh.sh"
hidevlab_ensure_agent_tailscale
host="$(hidevlab_pick_host)"
if [ "${1:-}" = "--export" ]; then
  echo "export SSH_HOST='${host}'"
else
  echo "${host}"
fi
