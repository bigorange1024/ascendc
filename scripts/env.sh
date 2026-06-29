#!/usr/bin/env bash
# Source before AscendC work:  source ~/ascendc/scripts/env.sh

export CANN_HOME="${CANN_HOME:-${HOME}/Ascend/cann}"
export ASCEND_HOME_DIR="${CANN_HOME}"
export ASCEND_CANN_PACKAGE_PATH="${CANN_HOME}"

# CANN set_env.sh uses ${VAR} expansion; avoid "unbound variable" under set -u
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH-}"
export PYTHONPATH="${PYTHONPATH-}"
export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH-}"
export PATH="${PATH-}"

_set_u_was_on=0
case "$-" in *u*) _set_u_was_on=1 ;; esac
set +u
# shellcheck source=/dev/null
source "${CANN_HOME}/set_env.sh"
if [[ "${_set_u_was_on}" -eq 1 ]]; then set -u; fi
unset _set_u_was_on

export PATH="${HOME}/.local/bin:${PATH}"

# Device-specific tikicpulib / simulator paths are set by examples/run.sh
# (e.g. ./run.sh cpu 910B4). Set ASCEND_DEVICE before sourcing if needed elsewhere.
