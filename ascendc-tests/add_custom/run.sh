#!/usr/bin/env bash
# Usage:
#   ./run.sh cpu [device]              # CPU 孪生调试 + golden 比对
#   ./run.sh sim [device]              # 默认：msprof 仿真 → prof_sim/ + OPPROF_*
#   ./run.sh sim [device] SIM_DIRECT=1 # 仅 CAModel 跑算子，不生成 OPPROF_*
#   SIM_DIRECT=1 ./run.sh sim [device] # 同上
#   ./run.sh sim-build [device]        # 仅编译 npu 二进制（WSL 无卡）
#   ./run.sh npu [device]              # 实机运行 add_custom_npu
#   ./run.sh all [device]              # cpu + sim
#
# sim 与 OPPROF_*：由 SIM_DIRECT 控制（默认 0）
#   SIM_DIRECT=0  → msprof op simulator，生成 prof_sim/ 与 OPPROF_*（若成功）
#   SIM_DIRECT=1  → 只执行 ./add_custom_npu，不跑 msprof，不生成 OPPROF_*
#
# Device examples: 910B4, ascend910B4, 910, 910B1, 310p
#
# 多环境分流（scripts/runtime_env.sh）：
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#   WSL 禁止 -r npu；说明见 docs/engineering/NPU真机环境说明.md
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
_ORIG_ARGS=("$@")
cd "${SCRIPT_DIR}"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# shellcheck source=/dev/null
source "${HOME}/ascendc/scripts/env.sh"

RUN_MODE="${1:-cpu}"
shift || true
DEVICE="${ASCEND_DEVICE:-910B4}"
# SIM_DIRECT：0=msprof+OPPROF（默认）  1=仅仿真跑算子
export SIM_DIRECT="${SIM_DIRECT:-0}"
while [[ $# -gt 0 ]]; do
  case "$1" in
    SIM_DIRECT=1|SIM_DIRECT) export SIM_DIRECT=1 ;;
    SIM_DIRECT=0) export SIM_DIRECT=0 ;;
    *)
      DEVICE="$1"
      ;;
  esac
  shift || true
done

# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
case "${RUN_MODE}" in
  cpu|sim|npu|auto|verify)
    export RUN_MODE
    runtime_env_detect
    runtime_env_print_banner
    if [ "${RUN_MODE}" = "verify" ]; then
      # add_custom 为位置参数（./run.sh cpu），不能把 -r 传给子进程
      runtime_env_detect
      for _m in $(runtime_env_verify_ladder); do
        echo "[runtime_env] === verify: ${_m} ==="
        if [ "${_m}" = "sim" ]; then export SIM_DIRECT="${SIM_DIRECT:-1}"; fi
        bash "${BASH_SOURCE[0]}" "${_m}" "${DEVICE}" || exit $?
      done
      echo "[runtime_env] verify PASS"
      exit 0
    elif [ "${RUN_MODE}" = "auto" ]; then
      RUN_MODE="$(runtime_env_resolve_mode auto)" || exit $?
      export RUN_MODE
      echo "[runtime_env] auto → ${RUN_MODE}"
      if [ "${RUN_MODE}" = "sim" ]; then export SIM_DIRECT="${SIM_DIRECT:-1}"; fi
    else
      RUN_MODE="$(runtime_env_resolve_mode "${RUN_MODE}")" || exit $?
      export RUN_MODE
      echo "[runtime_env] resolved RUN_MODE=${RUN_MODE}"
    fi
    ;;
esac

FILE_NAME="add_custom"
# shellcheck source=scripts/resolve_device.sh
source "${SCRIPT_DIR}/scripts/resolve_device.sh"
_resolve_device "${DEVICE}"

export ASCEND_HOME_DIR="${CANN_HOME}"
export PRINT_TIK_MEM_ACCESS=FALSE

# msprof 指标：sim 与实机支持的集合不同（见 msprof op simulator --help）
#   sim 仅支持: PipeUtilization, ResourceConflictRatio, PMSampling
#   实机还支持: MemoryUB, Memory, ArithmeticUtilization, ...
export MSPROF_AIC_METRICS_SIM="${MSPROF_AIC_METRICS_SIM:-PipeUtilization}"
export MSPROF_AIC_METRICS_NPU="${MSPROF_AIC_METRICS_NPU:-PipeUtilization,MemoryUB,Memory}"
export MSPROF_LAUNCH_COUNT_SIM="${MSPROF_LAUNCH_COUNT_SIM:-1}"
export MSPROF_LAUNCH_COUNT_NPU="${MSPROF_LAUNCH_COUNT_NPU:-8}"
# msprof --timeout 单位：分钟
export MSPROF_TIMEOUT_MIN="${MSPROF_TIMEOUT_MIN:-60}"
# 墙钟超时（秒），防止 CAModel 卡死；0=不限制
export MSPROF_WALL_TIMEOUT_SEC="${MSPROF_WALL_TIMEOUT_SEC:-1800}"

echo "=== Target device: ${DEVICE_LABEL} ==="
echo "    product_type=${SOC_VERSION}  simulator=${SIM_DEVICE}  arch=${CCEC_ARCH}"
if [[ "${RUN_MODE}" == "sim" || "${RUN_MODE}" == "all" ]]; then
  if [[ "${SIM_DIRECT}" == "1" ]]; then
    echo "    SIM_DIRECT=1 → 仅 CAModel 仿真，不运行 msprof，不生成 OPPROF_*"
  else
    echo "    SIM_DIRECT=0 → msprof 仿真性能，将生成 prof_sim/ 与 OPPROF_*（成功时）"
  fi
fi

prepare_data() {
  rm -rf build input output prof_sim prof_npu sim_log
  mkdir -p input output
  echo "=== Generate golden data ==="
  python3 gen_data.py
}

cmake_configure() {
  local mode="${1:-npu}"
  cmake -S . -B build \
    -Dsmoke_testcase="${FILE_NAME}" \
    -Dproduct_type="${SOC_VERSION}" \
    -Dsim_device="${SIM_DEVICE}" \
    -Dtikicpu_lib_device="${TIKICPU_LIB_DEVICE}" \
    -Dccec_aicore_arch="${CCEC_ARCH}" \
    -Drun_mode="${mode}" \
    -Dinstall_path="${CANN_HOME}"
}

build_npu() {
  local mode="${1:-npu}"
  cmake_configure "${mode}"
  echo "=== Build ${FILE_NAME}_npu (simulator: ${SIM_DEVICE}, arch: ${CCEC_ARCH}) ==="
  cmake --build build --target "${FILE_NAME}_npu" -j"$(nproc)"
  test -x "./${FILE_NAME}_npu"
  echo "[OK] ./${FILE_NAME}_npu"
}

compare_golden() {
  echo "=== Compare output vs golden ==="
  md5sum output/output_z.bin output/golden.bin
  if cmp -s output/output_z.bin output/golden.bin; then
    echo "[SUCCESS] output matches golden (${DEVICE_LABEL})"
  else
    echo "[FAILED] output differs from golden"
    exit 1
  fi
}

check_cpu_libs() {
  local wrong
  wrong="$(ldd "./${FILE_NAME}_cpu" 2>/dev/null | grep 'libcpudebug.so' | grep -v "${TIKICPU_CPU_LIB}" || true)"
  if [[ -n "${wrong}" ]]; then
    echo "[WARN] libcpudebug 未指向 ${TIKICPU_CPU_LIB}：" >&2
    echo "       ${wrong}" >&2
  fi
}

setup_cpu_env() {
  export LD_LIBRARY_PATH="${CANN_HOME}/tools/tikicpulib/lib/${TIKICPU_CPU_LIB}:${LD_LIBRARY_PATH}"
}

filter_ld_path_no_driver() {
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

setup_sim_env() {
  # 仿真：simulator 库优先；去掉 driver 路径，避免 ACL 107001（无卡/误连真机驱动）
  local sim_lib="${CANN_HOME}/toolkit/tools/simulator/${SIM_DEVICE}/lib"
  filter_ld_path_no_driver
  export LD_LIBRARY_PATH="${sim_lib}:${LD_LIBRARY_PATH}"
  export RUN_MODE=sim
  export CAMODEL_LOG_PATH="${SCRIPT_DIR}/sim_log"
  rm -rf "${CAMODEL_LOG_PATH}"
  mkdir -p "${CAMODEL_LOG_PATH}"
  export ASCEND_DEVICE_ID="${ASCEND_DEVICE_ID:-0}"
}

sim_progress_watch() {
  local prof_out="$1"
  local start
  start="$(date +%s)"
  while true; do
    sleep 20
    local elapsed=$(( $(date +%s) - start ))
    local log_n prof_n
    log_n="$(find "${CAMODEL_LOG_PATH}" -type f 2>/dev/null | wc -l)"
    prof_n="$(find "${prof_out}" -type f 2>/dev/null | wc -l)"
    echo "[sim 进行中 ${elapsed}s] sim_log 文件数=${log_n}  prof_sim 文件数=${prof_n}  (910B4 仿真较慢，可另开终端: ls -la sim_log prof_sim)"
  done
}

setup_npu_env() {
  export LD_LIBRARY_PATH="${CANN_HOME}/lib64:${LD_LIBRARY_PATH}"
}

require_msprof() {
  if ! command -v msprof >/dev/null 2>&1; then
    echo "ERROR: 未找到 msprof，请先 source ~/ascendc/scripts/env.sh" >&2
    exit 1
  fi
}

run_cpu() {
  prepare_data
  setup_cpu_env
  cmake_configure "cpu"
  cmake --build build --target "${FILE_NAME}_cpu" -j"$(nproc)"
  check_cpu_libs
  echo "=== Run ${FILE_NAME}_cpu (twin-debug) ==="
  "./${FILE_NAME}_cpu"
  compare_golden
}

run_sim_build_only() {
  prepare_data
  build_npu "sim"
  echo "[SUCCESS] sim-build only: ./${FILE_NAME}_npu (no msprof run)"
  echo "        On board: ./run.sh sim ${DEVICE}"
}

run_sim_direct() {
  echo "=== 直接运行 ${FILE_NAME}_npu（仿真库，无 msprof）==="
  setup_sim_env
  "./${FILE_NAME}_npu"
  compare_golden
}

run_sim() {
  prepare_data
  build_npu "sim"
  setup_sim_env

  if [[ "${SIM_DIRECT}" == "1" ]]; then
    run_sim_direct
    return
  fi

  require_msprof

  local prof_out="${SCRIPT_DIR}/prof_sim"
  rm -rf "${prof_out}"
  mkdir -p "${prof_out}"

  echo "=== msprof op simulator (soc=${SIM_DEVICE}) ==="
  echo "    aic-metrics=${MSPROF_AIC_METRICS_SIM}  launch-count=${MSPROF_LAUNCH_COUNT_SIM}"
  echo "    timeout=${MSPROF_TIMEOUT_MIN}min  wall-timeout=${MSPROF_WALL_TIMEOUT_SEC}s"
  echo "[NOTE] 910B4 CAModel 仿真常需数分钟～数十分钟，不是卡死；下方每 20s 打印进度"
  echo "       加快试跑: MSPROF_LAUNCH_COUNT_SIM=1 MSPROF_AIC_METRICS_SIM=PipeUtilization ./run.sh sim 910B4"

  sim_progress_watch "${prof_out}" &
  local watch_pid=$!
  local msprof_rc=0
  local msprof_cmd=(
    msprof op simulator
    "${SCRIPT_DIR}/${FILE_NAME}_npu"
    --soc-version="${SIM_DEVICE}"
    --output="${prof_out}"
    --aic-metrics="${MSPROF_AIC_METRICS_SIM}"
    --launch-count="${MSPROF_LAUNCH_COUNT_SIM}"
    --timeout="${MSPROF_TIMEOUT_MIN}"
  )

  if [[ "${MSPROF_WALL_TIMEOUT_SEC}" != "0" ]] && command -v timeout >/dev/null 2>&1; then
    timeout "${MSPROF_WALL_TIMEOUT_SEC}" "${msprof_cmd[@]}" || msprof_rc=$?
  else
    "${msprof_cmd[@]}" || msprof_rc=$?
  fi

  kill "${watch_pid}" 2>/dev/null || true
  wait "${watch_pid}" 2>/dev/null || true

  if [[ "${msprof_rc}" -ne 0 ]]; then
    echo "[FAILED] msprof op simulator 退出码=${msprof_rc}（124=墙钟超时）" >&2
    echo "        可先试: SIM_DIRECT=1 ./run.sh sim 910B4  仅跑算子不采性能" >&2
    exit 1
  fi

  echo ""
  # msprof 9.0 可能写到 OPPROF_* 目录，再链到 prof_sim
  local opprof_dir
  opprof_dir="$(ls -dt "${SCRIPT_DIR}"/OPPROF_* 2>/dev/null | head -1 || true)"
  if [[ -n "${opprof_dir}" && -d "${opprof_dir}" ]]; then
    # msprof 9.0 常以 750/root 创建，IDE/资源管理器可能看不到，统一放宽权限
    chmod -R u+rwX,go+rX "${opprof_dir}" 2>/dev/null || true
    ln -sfn "${opprof_dir}" "${prof_out}/latest" 2>/dev/null || true
    echo "[INFO] msprof 结果目录: ${opprof_dir}"
    echo "[INFO] 快捷入口: ${prof_out}/latest"
    ls -ld "${opprof_dir}" 2>/dev/null || true
  else
    echo "[WARN] 未找到 OPPROF_* 目录（仅 --output=prof_sim 时部分版本不落盘到 OPPROF）" >&2
  fi
  echo "[INFO] 性能数据目录: ${prof_out}"
  echo "[INFO] CAModel 日志: ${CAMODEL_LOG_PATH}"
  local csv_count
  csv_count="$(find "${prof_out}" "${opprof_dir:-/nonexistent}" -name '*.csv' 2>/dev/null | wc -l)"
  if [[ "${csv_count}" -eq 0 ]]; then
    echo "[WARN] prof_sim 下未找到 csv，性能采集可能未成功" >&2
  else
    echo "=== 性能 CSV 文件 ==="
    find "${prof_out}" -name '*.csv' 2>/dev/null
    echo "=== PipeUtilization 摘要（若有）==="
    find "${prof_out}" -name 'PipeUtilization.csv' -exec head -20 {} \; 2>/dev/null || true
  fi

  if [[ ! -f output/output_z.bin ]]; then
    echo "[FAILED] 算子未产出 output/output_z.bin" >&2
    echo "  ACL 107001=无效 Device：WSL/无卡请用 ./run.sh cpu；仿真需 run_mode=sim 重编（本脚本 sim 已 -Drun_mode=sim）" >&2
    echo "  有昇腾实机且要上板性能请: ./run.sh npu 910B4  或 RUN_WITH_MSPROF=1 ./run.sh npu 910B4" >&2
    exit 1
  fi
  compare_golden
}

run_npu() {
  prepare_data
  build_npu "npu"
  setup_npu_env

  if [[ "${RUN_WITH_MSPROF:-0}" == "1" ]]; then
    require_msprof
    local prof_out="${SCRIPT_DIR}/prof_npu"
    rm -rf "${prof_out}"
    mkdir -p "${prof_out}"
    echo "=== msprof op on device ==="
    echo "    aic-metrics=${MSPROF_AIC_METRICS_NPU}  launch-count=${MSPROF_LAUNCH_COUNT_NPU}"
    if ! msprof op \
      "${SCRIPT_DIR}/${FILE_NAME}_npu" \
      --output="${prof_out}" \
      --aic-metrics="${MSPROF_AIC_METRICS_NPU}" \
      --launch-count="${MSPROF_LAUNCH_COUNT_NPU}"; then
      echo "[FAILED] msprof op 失败" >&2
      exit 1
    fi
    echo "[INFO] 性能数据目录: ${prof_out}"
  else
    echo "=== Run ${FILE_NAME}_npu on NPU ==="
    "./${FILE_NAME}_npu"
  fi
  compare_golden
}

usage() {
  cat <<EOF
Usage: $0 <mode> [device] [SIM_DIRECT=1]

Modes:
  cpu        CPU 孪生调试（无 NPU）
  sim        仿真（见 SIM_DIRECT）
  sim-build  仅编译 npu 目标（WSL 无卡）
  npu        实机 ACL 运行（可选 RUN_WITH_MSPROF=1 采集性能）
  all        cpu + sim

Devices: 910, 910B1, 910B4 (default), 310p, ...

SIM_DIRECT（仅 sim / all 中的 sim 段）:
  0（默认）  msprof → prof_sim/ + OPPROF_*
  1          仅 ./add_custom_npu，不生成 OPPROF_*

  示例:
    ./run.sh sim 910B4
    SIM_DIRECT=1 ./run.sh sim 910B4
    ./run.sh sim 910B4 SIM_DIRECT=1

Env:
  ASCEND_DEVICE=910B4
  MSPROF_AIC_METRICS_SIM / MSPROF_LAUNCH_COUNT_SIM
  MSPROF_TIMEOUT_MIN=60   MSPROF_WALL_TIMEOUT_SEC=1800
  RUN_WITH_MSPROF=1       # npu 模式走 msprof op
  SIM_BUILD_ONLY=1        # 同 sim-build
EOF
}

case "${RUN_MODE}" in
  cpu) run_cpu ;;
  sim)
    if [[ "${SIM_BUILD_ONLY:-0}" == "1" ]]; then
      run_sim_build_only
    else
      run_sim
    fi
    ;;
  sim-build) run_sim_build_only ;;
  npu) run_npu ;;
  all) run_cpu; run_sim ;;
  -h|--help|help) usage ;;
  *)
    echo "ERROR: unknown mode '${RUN_MODE}'" >&2
    usage
    exit 1
    ;;
esac
