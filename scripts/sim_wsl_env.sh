#!/usr/bin/env bash
# WSL 无 NPU 驱动时 SIM 环境：
#   - stub libascend_dump（避免 dl_init FPE）
#   - devlib/device HAL（camodel 设备侧）
#   - /etc/ascend_install.info 重定向（无 sudo）
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
stub_src="${repo_root}/scripts/stub_libascend_dump.c"
redirect_src="${repo_root}/scripts/sim_install_redirect.c"
redirect_so="${repo_root}/scripts/libsim_install_redirect.so"
local_install_info="${repo_root}/scripts/local_etc/ascend_install.info"

sim_wsl_filter_ld_no_driver() {
  local entry new_ld=""
  local old_ifs="${IFS}"
  IFS=':'
  for entry in ${LD_LIBRARY_PATH:-}; do
    [[ -z "${entry}" ]] && continue
    [[ "${entry}" == *"/driver/"* ]] && continue
    new_ld="${new_ld:+$new_ld:}${entry}"
  done
  IFS="${old_ifs}"
  export LD_LIBRARY_PATH="${new_ld}"
}

sim_wsl_setup_stub_dump() {
  local case_dir="${1:?}"
  mkdir -p "${case_dir}/out/lib"
  if [[ ! -f "${case_dir}/out/lib/libascend_dump.so" ]] || [[ "${stub_src}" -nt "${case_dir}/out/lib/libascend_dump.so" ]]; then
    gcc -shared -fPIC -o "${case_dir}/out/lib/libascend_dump.so" "${stub_src}"
  fi
}

sim_wsl_setup_install_redirect() {
  mkdir -p "${repo_root}/scripts/local_etc"
  if [[ ! -f "${local_install_info}" ]]; then
    if [[ -f "${HOME}/Ascend/ascend_cann_install.info" ]]; then
      cp "${HOME}/Ascend/ascend_cann_install.info" "${local_install_info}"
    elif [[ -f "${CANN_HOME:-${ASCEND_HOME_PATH}}/ascend_cann_install.info" ]]; then
      cp "${CANN_HOME:-${ASCEND_HOME_PATH}}/ascend_cann_install.info" "${local_install_info}"
    fi
  fi
  if [[ ! -f "${redirect_so}" ]] || [[ "${redirect_src}" -nt "${redirect_so}" ]]; then
    gcc -shared -fPIC -o "${redirect_so}" "${redirect_src}" -ldl
  fi
}

sim_wsl_export_env() {
  local case_dir="${1:?}"
  local soc="${SOC_VERSION:-Ascend910B4}"
  sim_wsl_setup_install_redirect
  sim_wsl_setup_stub_dump "${case_dir}"
  sim_wsl_filter_ld_no_driver
  local sim_lib="${CANN_HOME:-${ASCEND_HOME_PATH}}/toolkit/tools/simulator/${soc}/lib"
  if [[ ! -d "${sim_lib}" ]]; then
    sim_lib="${CANN_HOME:-${ASCEND_HOME_PATH}}/tools/simulator/${soc}/lib"
  fi
  local dev_hal="${CANN_HOME:-${ASCEND_HOME_PATH}}/x86_64-linux/devlib/device"
  export LD_LIBRARY_PATH="${case_dir}/out/lib:${case_dir}/out/lib64:${dev_hal}:${sim_lib}:${LD_LIBRARY_PATH:-}"
  export LD_PRELOAD="${redirect_so}"
  export ASCENDC_REPO_ROOT="${repo_root}"
  export ASCEND_LATEST_INSTALL_PATH="${ASCEND_LATEST_INSTALL_PATH:-${HOME}/Ascend}"
  export ASCEND_DEVICE_ID="${ASCEND_DEVICE_ID:-0}"
  export SIM_DIRECT="${SIM_DIRECT:-1}"
}
