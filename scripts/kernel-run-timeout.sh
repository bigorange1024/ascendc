#!/usr/bin/env bash
# 带墙钟预算启动 AscendC host 可执行（CPU 孪生 / SIM CAModel）。
# 启动前按当前工作目录绑定 ASCEND_WORK_PATH，避免 shell 残留旧路径（重命名用例后写幽灵目录）。
set -euo pipefail
if [ "$#" -lt 1 ]; then
  echo "usage: kernel-run-timeout.sh <binary> [args...]" >&2
  exit 1
fi
_REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=/dev/null
source "${_REPO_ROOT}/scripts/camodel_sim_log.sh" "$(pwd)"
BIN="$1"
shift
BUDGET="${KERNEL_COMPUTE_BUDGET_SEC:-120}"
exec timeout --foreground "${BUDGET}" "${BIN}" "$@"
