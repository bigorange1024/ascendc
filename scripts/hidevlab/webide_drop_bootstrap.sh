#!/usr/bin/env bash
# 在 Cursor Agent 工作区生成「可粘贴到 HiDevLab WebIDE 的短安装块」。
# 人只粘贴短块 → 在 /workspace 落盘脚本 → 一行跑 bootstrap（同 cannlab 形态）。
# 勿整段粘贴超长 heredoc：WebIDE（VS Code）终端易崩，见手册 §终端启动失败。
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BOOT="${REPO_ROOT}/scripts/hidevlab/agent_bootstrap.sh"
DOG="${REPO_ROOT}/scripts/hidevlab/agent_watchdog.sh"

echo "【HiDevLab WebIDE · 短粘贴：落盘脚本】"
echo "说明: 分两刀。第一刀只写文件；第二刀再跑 bootstrap。避免长粘贴崩终端。"
echo
echo "<<<<<<< WEBIDE_PASTE_1_DROP_SCRIPTS"
cat <<'EOF'
set -euo pipefail
mkdir -p /workspace/ascendc/scripts/hidevlab
# 若本机已有仓内脚本则直接跳到 PASTE_2；否则用下面 curl/printf 由 Agent 另给 base64 包。
# 推荐：打开文件 /workspace/hidevlab_agent_bootstrap.sh（若 Agent 已通过其它通道写入）。
ls -la /workspace/ascendc/scripts/hidevlab/ 2>/dev/null || true
ls -la /workspace/hidevlab_*.sh 2>/dev/null || true
EOF
echo ">>>>>>> WEBIDE_PASTE_1_DROP_SCRIPTS"
echo
echo "<<<<<<< WEBIDE_PASTE_2_RUN（一行，同 cannlab）"
cat <<'EOF'
# 把 tskey 换成 Secrets 里的 TAILSCALE_AUTHKEY；禁止加 --ssh
# 默认不拉看门狗（ENABLE_WATCHDOG 勿乱开；曾误判 active 空转近 10h）
TS_AUTHKEY='tskey-auth-XXXX' \
  bash /workspace/ascendc/scripts/hidevlab/agent_bootstrap.sh
cat /workspace/.hidevlab_tailscale.env
tailscale status | head -15
EOF
echo ">>>>>>> WEBIDE_PASTE_2_RUN"
echo
echo "仓库内权威脚本:"
echo "  ${BOOT}"
echo "  ${DOG}"
echo "回传: .hidevlab_tailscale.env + status 前几行"
