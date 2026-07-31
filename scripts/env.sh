#!/usr/bin/env bash
# Source before AscendC work:  source <repo>/scripts/env.sh
#
# 探测 CANN 安装根并 source 其 set_env.sh，再回写 CANN_HOME。
# 覆盖（任选其一，优先显式）:
#   ASCEND_TOOLKIT_ENV_FILE=/path/to/set_env.sh
#   CANN_HOME=/path/to/cann-or-toolkit-root
#
# 说明：部分机器（如借入 NPU 上的 cann-9.1）source 后 CANN_HOME 为空，
# 仅设 ASCEND_HOME_PATH / ASCEND_TOOLKIT_HOME —— 本脚本必须回写 CANN_HOME。

_ascendc_env_candidates=()
if [[ -n "${ASCEND_TOOLKIT_ENV_FILE:-}" ]]; then
  _ascendc_env_candidates+=("${ASCEND_TOOLKIT_ENV_FILE}")
fi
if [[ -n "${CANN_HOME:-}" ]]; then
  _ascendc_env_candidates+=("${CANN_HOME}/set_env.sh")
fi
# 办公室 WSL 手工解包（须排在 /usr/local 之前，避免误吃别的机子上的旧包）
_ascendc_env_candidates+=(
  "${HOME}/Ascend/cann/set_env.sh"
  "${HOME}/Ascend/ascend-toolkit/latest/set_env.sh"
  "${HOME}/Ascend/ascend-toolkit/set_env.sh"
  "/usr/local/Ascend/cann/set_env.sh"
  "/usr/local/Ascend/ascend-toolkit/latest/set_env.sh"
  "/usr/local/Ascend/ascend-toolkit/set_env.sh"
)

_ascendc_set_env=""
for _cand in "${_ascendc_env_candidates[@]}"; do
  if [[ -n "${_cand}" && -f "${_cand}" ]]; then
    _ascendc_set_env="${_cand}"
    break
  fi
done

if [[ -z "${_ascendc_set_env}" ]]; then
  echo "[env.sh] ERROR: 未找到 CANN set_env.sh。可设置 ASCEND_TOOLKIT_ENV_FILE 或 CANN_HOME。" >&2
  echo "[env.sh] 已试候选:" >&2
  for _cand in "${_ascendc_env_candidates[@]}"; do
    [[ -n "${_cand}" ]] && echo "  - ${_cand}" >&2
  done
  unset _ascendc_env_candidates _ascendc_set_env _cand
  return 1 2>/dev/null || exit 1
fi

# CANN set_env.sh uses ${VAR} expansion; avoid "unbound variable" under set -u
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH-}"
export PYTHONPATH="${PYTHONPATH-}"
export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH-}"
export PATH="${PATH-}"

_set_u_was_on=0
case "$-" in *u*) _set_u_was_on=1 ;; esac
set +u
# shellcheck source=/dev/null
source "${_ascendc_set_env}"
if [[ "${_set_u_was_on}" -eq 1 ]]; then set -u; fi
unset _set_u_was_on

# 回写编译/链接根：优先 CANN 脚本已设的 ASCEND_*；否则用 set_env.sh 所在目录
if [[ -n "${ASCEND_HOME_PATH:-}" && -d "${ASCEND_HOME_PATH}" ]]; then
  export CANN_HOME="${ASCEND_HOME_PATH}"
elif [[ -n "${ASCEND_TOOLKIT_HOME:-}" && -d "${ASCEND_TOOLKIT_HOME}" ]]; then
  export CANN_HOME="${ASCEND_TOOLKIT_HOME}"
else
  export CANN_HOME="$(cd "$(dirname "${_ascendc_set_env}")" && pwd -P)"
fi
export ASCEND_HOME_PATH="${ASCEND_HOME_PATH:-${CANN_HOME}}"
export ASCEND_TOOLKIT_HOME="${ASCEND_TOOLKIT_HOME:-${CANN_HOME}}"
export ASCEND_HOME_DIR="${CANN_HOME}"
export ASCEND_CANN_PACKAGE_PATH="${CANN_HOME}"

export PATH="${HOME}/.local/bin:${PATH}"

unset _ascendc_env_candidates _ascendc_set_env _cand

# Device-specific tikicpulib / simulator paths are set by examples/run.sh
# (e.g. ./run.sh cpu 910B4). Set ASCEND_DEVICE before sourcing if needed elsewhere.
