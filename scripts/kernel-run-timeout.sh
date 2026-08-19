#!/usr/bin/env bash
# 带墙钟预算启动 AscendC host 可执行（CPU 孪生 / SIM CAModel / NPU）。
#
# KERNEL_COMPUTE_BUDGET_SEC：防挂死超时，由各用例 run.sh 设定默认值（非全仓 15s）。
# 本脚本未 export 时 fallback 120s，仅防漏设；性能定标见 docs/engineering/内核计算超时与性能定标.md
#
# 启动前按当前工作目录绑定 ASCEND_WORK_PATH，避免 shell 残留旧路径（重命名用例后写幽灵目录）。
#
# 墙钟：结束时向 stdout 打一行 `[wall_sec] <秒>`（kernel+host I/O，不含 cmake/gen_data）。
# 不依赖 /usr/bin/time（借入实机常未装 GNU time，`-r npu` 会静默没有 wall_sec）。
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

# bash 5+ 有 EPOCHREALTIME（微秒）；否则 GNU date %N。整数秒为最后兜底。
_kernel_now() {
  if [ -n "${EPOCHREALTIME+x}" ]; then
    printf '%s\n' "${EPOCHREALTIME}"
  elif date +%s.%N >/dev/null 2>&1; then
    date +%s.%N
  else
    date +%s
  fi
}

_t0="$(_kernel_now)"
# 用 timeout 包一层：挂死时先 SIGTERM（host 侧 DeviceGuard 可 Finalize），仍不退再 SIGKILL。
# SIGKILL 无法跑清理 → 同 ASCEND_DEVICE_ID 可能被驱动残留污染；杀死后须 npu-smi 看进程，
# 必要时对该卡 reset，再跑下一例（见 qa/2026-08-03）。
set +e
timeout --foreground "${BUDGET}" "${BIN}" "$@"
rc=$?
set -e
_t1="$(_kernel_now)"
_wall="$(awk -v s="${_t0}" -v e="${_t1}" 'BEGIN { printf "%.3f", (e - s) + 0 }')"
# 固定前缀，供 run.sh / msprof_run 解析；stdout 保证进 tee / 终端。
echo "[wall_sec] ${_wall}"
echo "[kernel-run-timeout] wall_sec=${_wall} budget=${BUDGET}s rc=${rc} bin=$(basename "${BIN}")"

if [ "${rc}" -eq 124 ]; then
  echo "[kernel-run-timeout] budget ${BUDGET}s exceeded (exit 124)。若刚 Ctrl+C/超时杀的是 NPU 进程：" >&2
  echo "  1) npu-smi info | sed -n '/Process/,\$p'  确认无残留" >&2
  echo "  2) 同 ASCEND_DEVICE_ID 上勿立刻开下一例；必要时对该卡 reset" >&2
  echo "  3) 换一张未被污染的卡只是绕开，不是根治" >&2
fi
exit "${rc}"
