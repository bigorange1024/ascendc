#!/usr/bin/env bash
# runtime_env.sh — 多环境运行时探测与 run 模式解析（WSL / Linux / Cloud / NPU）
#
# Usage（由各用例 run.sh source）:
#   source "${REPO_ROOT}/scripts/runtime_env.sh"
#   runtime_env_detect
#   runtime_env_print_banner
#   RUN_MODE="$(runtime_env_resolve_mode "${RUN_MODE}")"   # auto → 单档
#   # 或 verify：
#   runtime_env_run_verify "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"
#
# 导出（detect 后）:
#   ASCENDC_HOST_KIND   wsl | cloud | linux
#   ASCENDC_HAS_CANN    0|1
#   ASCENDC_HAS_SIM     0|1
#   ASCENDC_HAS_NPU     0|1（WSL 恒为 0）
#
# 用例可在 resolve 前设:
#   ASCENDC_CASE_SUPPORTS_NPU=0|1  （默认 1；一期若尚未接真机可显式 0）
#
# 权威说明: docs/engineering/NPU真机环境说明.md · AGENTS.md

runtime_env_is_wsl() {
  grep -qiE 'microsoft|wsl' /proc/version 2>/dev/null && return 0
  [[ -n "${WSL_DISTRO_NAME:-}" ]] && return 0
  return 1
}

runtime_env_cann_home() {
  if [[ -n "${CANN_HOME:-}" && -d "${CANN_HOME}" ]]; then
    printf '%s' "${CANN_HOME}"
    return 0
  fi
  if [[ -n "${ASCEND_HOME_PATH:-}" && -d "${ASCEND_HOME_PATH}" ]]; then
    printf '%s' "${ASCEND_HOME_PATH}"
    return 0
  fi
  if [[ -d "${HOME}/Ascend/cann" ]]; then
    printf '%s' "${HOME}/Ascend/cann"
    return 0
  fi
  if [[ -d "${HOME}/Ascend/ascend-toolkit/latest" ]]; then
    printf '%s' "${HOME}/Ascend/ascend-toolkit/latest"
    return 0
  fi
  # 借入 NPU 等：/usr/local/Ascend/cann → cann-x.y（与 ascend-toolkit/latest 常等价）
  if [[ -d /usr/local/Ascend/cann ]]; then
    printf '%s' /usr/local/Ascend/cann
    return 0
  fi
  if [[ -d /usr/local/Ascend/ascend-toolkit/latest ]]; then
    printf '%s' /usr/local/Ascend/ascend-toolkit/latest
    return 0
  fi
  return 1
}

runtime_env_has_sim_lib() {
  local cann soc sim_lib
  cann="$(runtime_env_cann_home)" || return 1
  soc="${SOC_VERSION:-Ascend910B4}"
  for sim_lib in \
    "${cann}/tools/simulator/${soc}/lib" \
    "${cann}/toolkit/tools/simulator/${soc}/lib"; do
    if [[ -d "${sim_lib}" ]]; then
      return 0
    fi
  done
  return 1
}

runtime_env_has_npu() {
  # WSL：规则禁止隐式/显式上板路径（verify 亦不跑 npu）
  if runtime_env_is_wsl; then
    return 1
  fi
  if ! command -v npu-smi >/dev/null 2>&1; then
    return 1
  fi
  if ! npu-smi info >/dev/null 2>&1; then
    return 1
  fi
  # 保守：再看设备节点（部分镜像有 npu-smi 包装假成功）
  if compgen -G '/dev/davinci*' >/dev/null 2>&1; then
    return 0
  fi
  # 无 /dev/davinci* 但 npu-smi 成功：仍视为可用（部分驱动布局不同）
  return 0
}

runtime_env_detect() {
  local cann=""

  if runtime_env_is_wsl; then
    export ASCENDC_HOST_KIND=wsl
  elif [[ -d /workspace ]] && { [[ -L "${HOME}/ascendc" ]] || [[ "$(readlink -f "${HOME}/ascendc" 2>/dev/null || true)" == /workspace* ]]; }; then
    export ASCENDC_HOST_KIND=cloud
  else
    export ASCENDC_HOST_KIND=linux
  fi

  if cann="$(runtime_env_cann_home)"; then
    export ASCENDC_HAS_CANN=1
    export CANN_HOME="${CANN_HOME:-${cann}}"
    export ASCEND_HOME_PATH="${ASCEND_HOME_PATH:-${cann}}"
  else
    export ASCENDC_HAS_CANN=0
  fi

  if [[ "${ASCENDC_HAS_CANN}" = "1" ]] && runtime_env_has_sim_lib; then
    export ASCENDC_HAS_SIM=1
  else
    export ASCENDC_HAS_SIM=0
  fi

  if runtime_env_has_npu; then
    export ASCENDC_HAS_NPU=1
  else
    export ASCENDC_HAS_NPU=0
  fi

  export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
}

runtime_env_print_banner() {
  echo "[runtime_env] host=${ASCENDC_HOST_KIND:-?} cann=${ASCENDC_HAS_CANN:-?} sim=${ASCENDC_HAS_SIM:-?} npu=${ASCENDC_HAS_NPU:-?} case_npu=${ASCENDC_CASE_SUPPORTS_NPU:-?} soc=${SOC_VERSION:-Ascend910B4}"
}

# 打印 verify 阶梯（空格分隔）：cpu [sim] [npu]
runtime_env_verify_ladder() {
  local parts=("cpu")
  if [[ "${ASCENDC_HAS_SIM:-0}" = "1" ]]; then
    parts+=("sim")
  else
    echo "[runtime_env] WARN: no CaModel SIM libs; verify skips sim" >&2
  fi
  if [[ "${ASCENDC_HAS_NPU:-0}" = "1" && "${ASCENDC_CASE_SUPPORTS_NPU:-1}" = "1" ]]; then
    parts+=("npu")
  elif [[ "${ASCENDC_HAS_NPU:-0}" = "1" && "${ASCENDC_CASE_SUPPORTS_NPU:-1}" != "1" ]]; then
    echo "[runtime_env] INFO: NPU present but ASCENDC_CASE_SUPPORTS_NPU=0; verify skips npu" >&2
  fi
  printf '%s' "${parts[*]}"
}

# 入参：请求模式；成功时 stdout 打印最终单档模式；非法则 stderr+非零
runtime_env_resolve_mode() {
  local want="${1:?mode}"
  case "${want}" in
    cpu)
      printf '%s\n' cpu
      return 0
      ;;
    sim)
      if [[ "${ASCENDC_HAS_SIM:-0}" != "1" ]]; then
        echo "[runtime_env] ERROR: -r sim but simulator libs not found (HAS_SIM=0)" >&2
        return 1
      fi
      printf '%s\n' sim
      return 0
      ;;
    npu)
      if runtime_env_is_wsl || [[ "${ASCENDC_HOST_KIND:-}" = "wsl" ]]; then
        echo "[runtime_env] ERROR: -r npu forbidden on WSL (use bare-metal NPU host)" >&2
        return 1
      fi
      if [[ "${ASCENDC_HAS_NPU:-0}" != "1" ]]; then
        echo "[runtime_env] ERROR: -r npu but no usable NPU (npu-smi / device)" >&2
        return 1
      fi
      if [[ "${ASCENDC_CASE_SUPPORTS_NPU:-1}" != "1" ]]; then
        echo "[runtime_env] ERROR: -r npu but this case sets ASCENDC_CASE_SUPPORTS_NPU=0" >&2
        return 1
      fi
      printf '%s\n' npu
      return 0
      ;;
    auto)
      if [[ "${ASCENDC_HAS_NPU:-0}" = "1" && "${ASCENDC_CASE_SUPPORTS_NPU:-1}" = "1" ]]; then
        printf '%s\n' npu
        return 0
      fi
      if [[ "${ASCENDC_HAS_NPU:-0}" = "1" && "${ASCENDC_CASE_SUPPORTS_NPU:-1}" != "1" ]]; then
        echo "[runtime_env] INFO: auto: NPU present but case_npu=0 → fall back" >&2
      fi
      if [[ "${ASCENDC_HAS_SIM:-0}" = "1" ]]; then
        printf '%s\n' sim
        return 0
      fi
      printf '%s\n' cpu
      return 0
      ;;
    verify)
      echo "[runtime_env] ERROR: verify is multi-step; use runtime_env_run_verify" >&2
      return 1
      ;;
    *)
      echo "[runtime_env] ERROR: unknown run mode '${want}' (cpu|sim|npu|auto|verify)" >&2
      return 1
      ;;
  esac
}

# 从参数列表去掉 -r/--run-mode 及其值，stdout 打印剩余（NUL 不安全；用数组间接）
# 用法: runtime_env_run_verify "$0" "${_ORIG_ARGS[@]}"
runtime_env_run_verify() {
  local self="${1:?}"
  shift
  local mode filtered skip a rc=0
  local -a ladder

  runtime_env_detect
  runtime_env_print_banner
  # shellcheck disable=SC2207
  ladder=($(runtime_env_verify_ladder))
  echo "[runtime_env] verify ladder: ${ladder[*]}"

  for mode in "${ladder[@]}"; do
    filtered=()
    skip=0
    for a in "$@"; do
      if [[ "${skip}" = "1" ]]; then
        skip=0
        continue
      fi
      case "${a}" in
        -r | --run-mode) skip=1; continue ;;
        *) filtered+=("${a}") ;;
      esac
    done
    echo "[runtime_env] === verify: -r ${mode} ==="
    # auto/verify 选中 sim 时强制 SIM_DIRECT（除非调用方已显式设置 SIM_DIRECT）
    if [[ "${mode}" = "sim" && -z "${SIM_DIRECT+x}" ]]; then
      export SIM_DIRECT=1
    elif [[ "${mode}" = "sim" ]]; then
      export SIM_DIRECT="${SIM_DIRECT:-1}"
    fi
    if ! bash "${self}" -r "${mode}" "${filtered[@]}"; then
      rc=$?
      echo "[runtime_env] verify FAILED at mode=${mode} rc=${rc}" >&2
      return "${rc}"
    fi
  done
  echo "[runtime_env] verify PASS (${ladder[*]})"
  return 0
}

# run.sh 在 getopt 后调用：处理 auto / verify / npu 门禁；修改调用方 RUN_MODE。
# 若为 verify：跑阶梯后 exit（须传入脚本路径与原始 argv）。
#   _ORIG_ARGS=("$@")   # 须在 getopt 消费 argv 之前保存
#   … getopt …
#   source …/runtime_env.sh
#   runtime_env_dispatch "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"
# 调用方可在此前设 ASCENDC_CASE_SUPPORTS_NPU=0|1
runtime_env_dispatch() {
  local self="${1:?}"
  shift
  local want="${RUN_MODE}"

  case "${want}" in
    verify)
      # detect/banner 由 runtime_env_run_verify 统一打出
      runtime_env_run_verify "${self}" "$@"
      exit $?
      ;;
    auto)
      runtime_env_detect
      runtime_env_print_banner
      RUN_MODE="$(runtime_env_resolve_mode auto)" || exit $?
      export RUN_MODE
      echo "[runtime_env] auto → ${RUN_MODE} (host=${ASCENDC_HOST_KIND})"
      # auto 落 sim：默认 SIM_DIRECT=1（省 msprof）；调用方已 export SIM_DIRECT 则保留
      if [[ "${RUN_MODE}" = "sim" ]]; then
        export SIM_DIRECT="${SIM_DIRECT:-1}"
        export ASCEND_DEVICE_ID=0
        echo "[runtime_env] SIM 强制 ASCEND_DEVICE_ID=0"
      elif [[ "${RUN_MODE}" = "npu" ]]; then
        # shellcheck source=/dev/null
        source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/npu_device_map.sh"
        npu_device_map_apply "$(cd "$(dirname "${self}")" && pwd)"
      fi
      ;;
    cpu | sim | npu)
      runtime_env_detect
      runtime_env_print_banner
      RUN_MODE="$(runtime_env_resolve_mode "${want}")" || exit $?
      export RUN_MODE
      echo "[runtime_env] resolved RUN_MODE=${RUN_MODE}"
      # npu：按用例树分卡（stable=1 / examples=2 / tests=3）；sim：CAModel 仅设备 0
      _runtime_env_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
      if [[ "${RUN_MODE}" = "npu" ]]; then
        # shellcheck source=/dev/null
        source "${_runtime_env_dir}/npu_device_map.sh"
        npu_device_map_apply "$(cd "$(dirname "${self}")" && pwd)"
      elif [[ "${RUN_MODE}" = "sim" ]]; then
        export ASCEND_DEVICE_ID=0
        echo "[runtime_env] SIM 强制 ASCEND_DEVICE_ID=0"
      fi
      unset _runtime_env_dir
      ;;
    *)
      echo "[runtime_env] ERROR: bad RUN_MODE=${want}" >&2
      exit 1
      ;;
  esac
}
