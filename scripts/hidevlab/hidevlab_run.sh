#!/usr/bin/env bash
# Cursor Agent 侧：经 Tailscale SOCKS 在 HiDevLab 上执行一条命令（仿 ~/.ssh/cannlab_npu.sh）。
# 用法：
#   bash scripts/hidevlab/hidevlab_run.sh 'echo CONNECTED'
#   bash scripts/hidevlab/hidevlab_run.sh 'cd /workspace/ascendc/ascendc-tests/add_custom && ASCEND_DEVICE_ID=0 CANNLAB=1 bash run.sh -r npu -v Ascend910B3'
#
# 可选：先做加法对拍健康检查（猎挂后 Process 空但仍会静默算错）
#   HIDEVLAB_NPU_HEALTH=1 bash scripts/hidevlab/hidevlab_run.sh '…你的 NPU 命令…'
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib_ssh.sh
source "${SCRIPT_DIR}/lib_ssh.sh"

CMD="${*:-}"
if [ -z "${CMD}" ]; then
  echo "Usage: hidevlab_run.sh '<remote bash command>'" >&2
  exit 2
fi

hidevlab_ensure_agent_tailscale
hidevlab_pick_host >/dev/null
# 心跳文件仅给「显式启用的」看门狗用；不表示平台计费保活。Agent 空闲时不应依赖看门狗关机。
PRELUDE='export LD_LIBRARY_PATH=/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:${LD_LIBRARY_PATH:-}; for f in /usr/local/Ascend/cann-9.1.0/set_env.sh /usr/local/Ascend/cann/set_env.sh /usr/local/Ascend/ascend-toolkit/set_env.sh; do if [ -f "$f" ]; then set +u; source "$f" >/dev/null 2>&1; set -u; break; fi; done; export ASCEND_DEVICE_ID="${ASCEND_DEVICE_ID:-0}"; export CANNLAB=1; export PATH="/usr/local/python3.12.13/bin:${PATH}"; if [ "${HIDEVLAB_TOUCH_HEARTBEAT:-0}" = "1" ]; then touch /workspace/.agent_heartbeat 2>/dev/null || true; fi; '

if [ "${HIDEVLAB_NPU_HEALTH:-0}" = "1" ]; then
  echo "[hidevlab_run] HIDEVLAB_NPU_HEALTH=1 → 先跑加法对拍健康检查"
  HEALTH_CMD='bash /workspace/ascendc/scripts/hidevlab/npu_golden_health.sh'
  hidevlab_ssh bash -lc "${PRELUDE}${HEALTH_CMD}" || {
    echo "[hidevlab_run] 健康检查失败：NPU 可能静默算错。请控制台关机→启动后再试；本命令未执行。" >&2
    exit 1
  }
fi

hidevlab_ssh bash -lc "${PRELUDE}${CMD}"
