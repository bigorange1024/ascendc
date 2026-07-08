#!/usr/bin/env bash
# CaModel SIM 日志目录与 stray dump 收拢（各 ascendc-tests run.sh source 本脚本）。
# kernel-run-timeout.sh 也会在 exec 前按 $(pwd) 再次 source，CPU/SIM 均绑定当前目录。
# Usage:
#   source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
#   ... kernel ...
#   camodel_sim_collect_stray "${CURRENT_DIR}"

_camodel_case_dir="${1:-.}"
if [[ ! -d "${_camodel_case_dir}" ]]; then
  echo "[camodel_sim_log] WARN: case dir not found: ${_camodel_case_dir}" >&2
fi

# 始终绑定当前用例目录，勿用 ${VAR:-} 保留 shell 里残留的旧路径（重命名用例后会写到幽灵目录）
export CAMODEL_LOG_PATH="${_camodel_case_dir}/sim_log"
mkdir -p "${CAMODEL_LOG_PATH}"

export ASCEND_PROCESS_LOG_PATH="${CAMODEL_LOG_PATH}"

# ASCEND_WORK_PATH 是 ADX DFX dump 的工作根。设置它会让 libascend_dump.so 的 DumpManager
# 在**静态初始化期**就 EnableDfxDumper → 提前 boot CAModel runtime；个别探针二进制会在此路径命中
# CANN 侧 bug（cce::runtime::Config::InitHardwareInfo950() 对空 unordered_map 取模除零 → SIGFPE，
# 崩在 main 之前，与 kernel 数值无关，见 qa 纪要）。SIM_DIRECT 金标跑本身不需要 ADX dump（日志已由
# CAMODEL_LOG_PATH / ASCEND_PROCESS_LOG_PATH 落到 sim_log），故提供默认关闭的 opt-out：
# 受影响用例在 run.sh 里 `export CAMODEL_SKIP_ADX_WORK_PATH=1` 即可跳过（默认仍导出，行为不变）。
if [[ "${CAMODEL_SKIP_ADX_WORK_PATH:-0}" == "1" ]]; then
  unset ASCEND_WORK_PATH
else
  export ASCEND_WORK_PATH="${CAMODEL_LOG_PATH}"
fi

camodel_sim_collect_stray() {
  local root="${1:-.}"
  local dest="${root}/sim_log"
  mkdir -p "${dest}"
  local moved=0
  shopt -s nullglob
  for f in "${root}"/core*.dump "${root}"/profile_*_log*.toml "${root}"/cceprint "${root}"/npuchk; do
    if [[ -e "${f}" ]]; then
      mv -f "${f}" "${dest}/" 2>/dev/null && moved=$((moved + 1)) || true
    fi
  done
  shopt -u nullglob
  if [[ "${moved}" -gt 0 ]]; then
    echo "[camodel_sim_log] moved ${moved} stray artifact(s) -> ${dest}"
  fi
}
