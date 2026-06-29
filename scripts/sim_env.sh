#!/usr/bin/env bash
# AscendC CaModel SIM 环境 — 与 pass-* / add_custom 一致。
#
# 要点（见 docs/engineering/环境复现与开发指南.md §7、§10）：
#   - simulator 库 prepend；filter 掉 driver/ 路径（避免 ACL 107001）
#   - camodel_sim_log.sh 由 run.sh 在 kernel 前 source
#   - WSL 无 NPU 时 libascend_dump 静态初始化 FPE → case/out/lib 内 stub 优先加载
#   - CANN 读取 /etc/ascend_install.info → 完整安装会创建；手动装包需一次性 symlink
#
# Usage:
#   source "${REPO_ROOT}/scripts/sim_env.sh"
#   sim_env_export "${CASE_DIR}" "${REPO_ROOT}"

sim_env_filter_ld_no_driver() {
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

sim_env_simulator_lib() {
  local cann="${CANN_HOME:-${ASCEND_HOME_PATH:-}}"
  local soc="${SOC_VERSION:-Ascend910B4}"
  local sim_lib="${cann}/tools/simulator/${soc}/lib"
  if [[ ! -d "${sim_lib}" ]]; then
    sim_lib="${cann}/toolkit/tools/simulator/${soc}/lib"
  fi
  if [[ ! -d "${sim_lib}" ]]; then
    echo "[sim_env] ERROR: simulator lib not found for ${soc} under ${cann}" >&2
    return 1
  fi
  printf '%s' "${sim_lib}"
}

sim_env_stub_dump() {
  local case_dir="$1"
  local repo_root="$2"
  local stub_src="${repo_root}/scripts/stub_libascend_dump.cpp"
  local stub_so="${case_dir}/out/lib/libascend_dump.so"
  if [[ ! -f "${stub_src}" ]]; then
    echo "[sim_env] WARN: stub source missing: ${stub_src}" >&2
    return 0
  fi
  mkdir -p "${case_dir}/out/lib"
  if [[ ! -f "${stub_so}" ]] || [[ "${stub_src}" -nt "${stub_so}" ]]; then
    g++ -shared -fPIC -o "${stub_so}" "${stub_src}"
  fi
}

sim_env_check_install_info() {
  if [[ -f /etc/ascend_install.info ]]; then
    return 0
  fi
  local src="${HOME}/Ascend/ascend_cann_install.info"
  if [[ ! -f "${src}" ]]; then
    src="${CANN_HOME:-}/ascend_cann_install.info"
  fi
  echo "[sim_env] ERROR: /etc/ascend_install.info 不存在，aclInit 会失败。" >&2
  if [[ -f "${src}" ]]; then
    echo "[sim_env] 一次性修复（完整 CANN 安装通常已创建该文件）：" >&2
    echo "  sudo ln -sf ${src} /etc/ascend_install.info" >&2
  else
    echo "[sim_env] 请先安装 CANN 或生成 ~/Ascend/ascend_cann_install.info。" >&2
  fi
  return 1
}

sim_env_export() {
  local case_dir="${1:?case_dir}"
  local repo_root="${2:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
  local cann_lib64="${CANN_HOME:-${ASCEND_HOME_PATH:-}}/lib64"
  local sim_lib

  # 清掉 shell 里上一次用例残留，避免 SIM 写日志到已删除/重命名目录
  unset CAMODEL_LOG_PATH ASCEND_WORK_PATH ASCEND_PROCESS_LOG_PATH

  sim_env_check_install_info || return 1
  sim_env_stub_dump "${case_dir}" "${repo_root}"
  sim_env_filter_ld_no_driver
  sim_lib="$(sim_env_simulator_lib)" || return 1

  export ASCEND_LATEST_INSTALL_PATH="${ASCEND_LATEST_INSTALL_PATH:-${HOME}/Ascend}"
  export SIM_DIRECT="${SIM_DIRECT:-1}"
  unset LD_PRELOAD
  export LD_LIBRARY_PATH="${case_dir}/out/lib:${case_dir}/out/lib64:${sim_lib}:${cann_lib64}:${LD_LIBRARY_PATH:-}"
}
