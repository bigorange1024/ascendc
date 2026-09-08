#!/usr/bin/env bash
# which_npu.sh — 打印当前 tailnet 上 online 的 CANNLab 节点（主机名 + IP）
# 每次云主机重启都可能换 IP / MagicDNS（cannlab-npu、cannlab-npu-1…），勿写死。
#
#   bash scripts/cannlab/which_npu.sh           # 人类可读
#   eval "$(bash scripts/cannlab/which_npu.sh --export)"  # 导出 SSH_HOST
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib_ssh.sh
source "${SCRIPT_DIR}/lib_ssh.sh"

cannlab_ensure_tailscale
unset SSH_HOST
# 强制扫描（忽略环境里旧的 SSH_HOST）
SSH_HOST_FORCE=""
cannlab_pick_host

ip=""
st="$(tailscale --socket="${TS_SOCK}" status 2>/dev/null || true)"
while IFS= read -r line; do
  echo "${line}" | grep -q offline && continue
  h="$(echo "${line}" | awk '{print $2}')"
  if [ "${h}" = "${SSH_HOST}" ]; then
    ip="$(echo "${line}" | awk '{print $1}')"
    break
  fi
done <<< "${st}"

if [ "${1:-}" = "--export" ]; then
  printf 'export SSH_HOST=%q\n' "${SSH_HOST}"
  [ -n "${ip}" ] && printf 'export SSH_HOST_IP=%q\n' "${ip}"
  exit 0
fi

echo "SSH_HOST=${SSH_HOST}"
echo "SSH_HOST_IP=${ip:-unknown}"
echo "hint: 脚本已自动 pick；一般不必 export。覆盖用 SSH_HOST_FORCE=..."
