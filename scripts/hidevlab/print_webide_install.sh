#!/usr/bin/env bash
# 把仓库内 bootstrap/watchdog 打成 base64 短引导，供 WebIDE「先落盘再执行」。
# Agent 在能连上的通道（或聊天）里把输出给人；人 WebIDE 只跑短命令，避免终端崩。
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOOT_B64="$(base64 -w0 "${SCRIPT_DIR}/agent_bootstrap.sh")"
DOG_B64="$(base64 -w0 "${SCRIPT_DIR}/agent_watchdog.sh")"

cat <<EOF
# ===== 粘贴到 HiDevLab WebIDE（短）=====
set -euo pipefail
mkdir -p /workspace/ascendc/scripts/hidevlab
echo '${DOG_B64}' | base64 -d > /workspace/ascendc/scripts/hidevlab/agent_watchdog.sh
echo '${BOOT_B64}' | base64 -d > /workspace/ascendc/scripts/hidevlab/agent_bootstrap.sh
chmod +x /workspace/ascendc/scripts/hidevlab/agent_*.sh
ls -la /workspace/ascendc/scripts/hidevlab/
echo "落盘 OK。下一行换成真 key 后执行："
echo "TS_AUTHKEY='tskey-auth-XXXX' bash /workspace/ascendc/scripts/hidevlab/agent_bootstrap.sh"
EOF
