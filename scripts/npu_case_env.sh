#!/usr/bin/env bash
# npu_case_env.sh — 用例 run.sh 共用：CANN env.sh + 分卡 + cpu/sim/npu LD_LIBRARY_PATH。
#
# 背景（2026-08-19）：768/512/1024 incubating 与 frozen correctness 仍 source
# ${HOME}/ascendc/scripts/env.sh，借入实机仓库不在该路径时会静默落到错误 toolkit，
# 且 npu 未设 lib64、未走 npu_device_map。本函数与 1024 stable run.sh 对齐。
#
# 调用方须已设置：REPO_ROOT、CURRENT_DIR、RUN_MODE、SOC_VERSION。
# 成功后导出：_ASCEND_INSTALL_PATH、CANN_HOME、ASCEND_HOME_PATH、ASCEND_TOOLKIT_HOME，
# npu 另导出 ASCEND_DEVICE_ID（显式优先，否则按树分卡）。
#
# 用法（getopt + runtime_env_dispatch 之后）：
#   # shellcheck source=/dev/null
#   source "${REPO_ROOT}/scripts/npu_case_env.sh"
#   npu_case_bootstrap

npu_case_bootstrap() {
    if [ -z "${REPO_ROOT:-}" ] || [ ! -d "${REPO_ROOT}/scripts" ]; then
        echo "[npu_case_env] ERROR: REPO_ROOT 未设置或不含 scripts/" >&2
        return 1
    fi
    if [ -z "${CURRENT_DIR:-}" ] || [ ! -d "${CURRENT_DIR}" ]; then
        echo "[npu_case_env] ERROR: CURRENT_DIR 未设置" >&2
        return 1
    fi
    if [ -z "${RUN_MODE:-}" ]; then
        echo "[npu_case_env] ERROR: RUN_MODE 未设置" >&2
        return 1
    fi
    # Host ReadFile("./input/…") / camodel $(pwd) 绑定：须在用例根执行
    cd "${CURRENT_DIR}" || return 1

    local _had_errexit=0
    case $- in *e*) _had_errexit=1 ;; esac
    set +e
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/env.sh"
    local _env_rc=$?
    [ "${_had_errexit}" = "1" ] && set -e
    if [ "${_env_rc}" -ne 0 ]; then
        echo "[npu_case_env] ERROR: source ${REPO_ROOT}/scripts/env.sh failed (rc=${_env_rc})" >&2
        return 1
    fi

    if [ -n "${ASCEND_INSTALL_PATH:-}" ]; then
        _ASCEND_INSTALL_PATH="${ASCEND_INSTALL_PATH}"
    elif [ -n "${CANN_HOME:-}" ] && [ -d "${CANN_HOME}" ]; then
        _ASCEND_INSTALL_PATH="${CANN_HOME}"
    elif [ -n "${ASCEND_HOME_PATH:-}" ] && [ -d "${ASCEND_HOME_PATH}" ]; then
        _ASCEND_INSTALL_PATH="${ASCEND_HOME_PATH}"
    else
        echo "[npu_case_env] ERROR: CANN_HOME / ASCEND_HOME_PATH 未设置" >&2
        return 1
    fi
    if ! command -v ccec >/dev/null 2>&1; then
        echo "[npu_case_env] ERROR: 未找到 ccec。CANN_HOME=${CANN_HOME:-}" >&2
        return 1
    fi
    export ASCEND_TOOLKIT_HOME="${_ASCEND_INSTALL_PATH}"
    export ASCEND_HOME_PATH="${_ASCEND_INSTALL_PATH}"
    export CANN_HOME="${_ASCEND_INSTALL_PATH}"

    local _soc="${SOC_VERSION:-Ascend910B4}"
    if [ "${RUN_MODE}" = "npu" ]; then
        # shellcheck source=/dev/null
        source "${REPO_ROOT}/scripts/npu_device_map.sh"
        npu_device_map_apply "${CURRENT_DIR}"
        export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
    elif [ "${RUN_MODE}" = "sim" ]; then
        export ASCEND_DEVICE_ID=0
        export SIM_DIRECT="${SIM_DIRECT:-1}"
    elif [ "${RUN_MODE}" = "cpu" ]; then
        export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${_soc}:${_ASCEND_INSTALL_PATH}/tools/simulator/${_soc}/lib:${LD_LIBRARY_PATH:-}"
    fi
    return 0
}
