#!/usr/bin/env bash
# stable-fips203-mlkem-kem-encaps-k4 — FIPS 203 Alg.20/17 Encaps（vendored Encrypt + 设备 H/G）
#
# customspec：stable-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex
# registry：docs/specs/fips203-mlkem1024-kem-encaps-baseline-registry.md
# 生产 I/O：input/{ek_kem,m,lut_*} → output/{c,K}.bin
#   $r$ 由设备 G 写出，Host 禁止预填 $r$ 为生产契约
# SIM：2 launch（f203_kem_enc_prep → f203_encrypt_l18_l19）
# CPU：5 launch（同 Encrypt 分叉；第 1 次为 kem prep）
#
# Usage（默认 = 全量生产路径；无需手动 export SIM_DIRECT）：
#   cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4   # 推荐；脚本也会 cd 到本目录
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4          # 内置 SIM_DIRECT=1（CAModel）；WSL/Cloud 勿再手写
#   bash run.sh -r auto -v Ascend910B4         # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4       # cpu → SIM_DIRECT sim [→ npu，非 WSL]
#
# 调试（非默认）:
#   KEM_ENCAPS_FORCE_REBUILD=1 …
#   SIM_DIRECT=0 … -r sim …                   # msprof + OPPROF_*（慢）
#   M_FILE=… / M_HEX=… / M_DEFAULT_HEX=… / EK_KEM_SRC=…
#   （默认 m=urandom；禁止默认可全 0。M_RANDOM 已废，勿再依赖）
#   CMAKE_BUILD_JOBS=2                        # 限并行，WSL 友好（已是默认）
#   bash run.sh -r npu …                      # 仅真机；WSL 由 runtime_env 拒绝
#   默认直跑结束会打印 [run_metrics] wall_sec=… 与 [npu_launch] 逐 launch
#   RUN_WITH_MSPROF=1 MSPROF_MODE=app … -r npu # 整进程 profiling（勿用 op 重放当整算子时间）
#
# SIM 三环境：WSL 装 dump 桩（sim_env.sh）；Cloud/非 WSL 不装桩 + CAMODEL_SKIP_ADX_WORK_PATH。
# 分流：scripts/runtime_env.sh · scripts/sim_env.sh · docs/engineering/NPU真机环境说明.md

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# Host ReadFile("./input/…") / camodel $(pwd) 绑定：须在用例根执行（WSL 从仓库根绝对路径调用也要落这里）
cd "${CURRENT_DIR}"
_ORIG_ARGS=("$@")

if [ "${KEM_ENCAPS_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_liboqs_kem_encaps.log" 2>&1
fi

# stable-* → stable → examples → repo
REPO_ROOT="$(
  _d="${CURRENT_DIR}"
  while [ "${_d}" != "/" ]; do
    if [ -f "${_d}/AGENTS.md" ] && [ -d "${_d}/scripts" ]; then
      printf '%s\n' "${_d}"
      break
    fi
    _d="$(dirname "${_d}")"
  done
)"
if [ -z "${REPO_ROOT}" ] || [ ! -d "${REPO_ROOT}/scripts" ]; then
  echo "[ERROR] cannot locate repo root from ${CURRENT_DIR}" >&2
  exit 1
fi

export KEM_ENCAPS_VERIFY="${KEM_ENCAPS_VERIFY:-1}"
export KEM_ENCAPS_SKIP_REBUILD="${KEM_ENCAPS_SKIP_REBUILD:-${KEM_SKIP_REBUILD:-1}}"
export KEM_ENCAPS_FORCE_REBUILD="${KEM_ENCAPS_FORCE_REBUILD:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEM_ENCAPS_KERNEL_BUDGET_SEC:-900}"

BUILD_TYPE="Debug"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"
INSTALL_PREFIX=""

SHORT=r:,v:,i:,b:,p:
LONG=run-mode:,soc-version:,install-path:,build-type:,install-prefix:
OPTS=$(getopt -a --options "$SHORT" --longoptions "$LONG" -- "$@")
eval set -- "$OPTS"
while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    -i | --install-path) ASCEND_INSTALL_PATH="$2"; shift 2 ;;
    -b | --build-type) BUILD_TYPE="$2"; shift 2 ;;
    -p | --install-prefix) INSTALL_PREFIX="$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[ERROR] Unexpected option: $1"; exit 1 ;;
    esac
done

# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
runtime_env_dispatch "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"

BUILD_PROFILE="${KEM_ENCAPS_BUILD_PROFILE:-prod}"
BUILD_DIR="${CURRENT_DIR}/build_${BUILD_PROFILE}_${RUN_MODE}"
if [ -z "${INSTALL_PREFIX}" ]; then
    INSTALL_PREFIX="${CURRENT_DIR}/out_${BUILD_PROFILE}_${RUN_MODE}"
fi

# CANN：WSL 常见 ~/Ascend/cann 或 ~/ascendc/scripts/env.sh
# CANN：统一 ${REPO_ROOT}/scripts/env.sh（多候选 set_env + 回写 CANN_HOME）；禁止写死 ~/ascendc，
# 否则仓库不在 $HOME/ascendc 的机器（如借入实机）会静默落到错误的 toolkit。
# 保存/恢复 errexit：本段靠返回码判断 source 结果，不改变脚本原有的 set -e 状态。
_had_errexit=0
case $- in *e*) _had_errexit=1 ;; esac
set +e
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/env.sh"
_env_rc=$?
[ "${_had_errexit}" = "1" ] && set -e
if [ "${_env_rc}" -ne 0 ]; then
    echo "[ERROR] source ${REPO_ROOT}/scripts/env.sh failed (rc=${_env_rc})" >&2
    exit 1
fi
if [ -n "${ASCEND_INSTALL_PATH:-}" ]; then
    _ASCEND_INSTALL_PATH="${ASCEND_INSTALL_PATH}"
elif [ -n "${CANN_HOME:-}" ] && [ -d "${CANN_HOME}" ]; then
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -n "${ASCEND_HOME_PATH:-}" ] && [ -d "${ASCEND_HOME_PATH}" ]; then
    _ASCEND_INSTALL_PATH="${ASCEND_HOME_PATH}"
else
    echo "[ERROR] CANN_HOME / ASCEND_HOME_PATH 未设置" >&2
    exit 1
fi
if ! command -v ccec >/dev/null 2>&1; then
    echo "[ERROR] 未找到 ccec。CANN_HOME=${CANN_HOME:-}" >&2
    exit 1
fi
export ASCEND_TOOLKIT_HOME="${_ASCEND_INSTALL_PATH}"
export ASCEND_HOME_PATH="${_ASCEND_INSTALL_PATH}"
export CANN_HOME="${_ASCEND_INSTALL_PATH}"

# 实机 ACL 设备号：npu 按树分卡（stable=1 / examples=2 / tests=3，见 npu_device_map.sh）；显式 ASCEND_DEVICE_ID 优先。SIM 强制 0
if [ "${RUN_MODE}" = "npu" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/npu_device_map.sh"
    npu_device_map_apply "${CURRENT_DIR}"
elif [ "${RUN_MODE}" = "sim" ]; then
    export ASCEND_DEVICE_ID=0
fi

# 须在 source setenv 之后再 set -e（setenv 内可能有非零 grep）
set -euo pipefail

if [ "${RUN_MODE}" = "sim" ]; then
    # 默认 = CAModel 金标；WSL/Cloud 均勿要求用户手动 SIM_DIRECT=1
    export SIM_DIRECT="${SIM_DIRECT:-1}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "npu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
fi

echo "[kem_encaps] RUN_MODE=${RUN_MODE} profile=${BUILD_PROFILE} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC} SIM_DIRECT=${SIM_DIRECT:-n/a}"

_build_stamp="${BUILD_DIR}/.kem_encaps_run_mode"
_build_tag="${BUILD_PROFILE}:${RUN_MODE}"
_need_build=1
if [ "${KEM_ENCAPS_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${KEM_ENCAPS_SKIP_REBUILD}" = "1" ] && [ -x "${INSTALL_PREFIX}/bin/ascendc_kem_encaps_bbit" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_kem_build() {
    if [ "${KEM_ENCAPS_FORCE_REBUILD}" = "1" ] || \
       { [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" != "${_build_tag}" ]; }; then
        rm -rf "${BUILD_DIR}" "${INSTALL_PREFIX}"
    fi
    mkdir -p "${BUILD_DIR}"
    cmake -B "${BUILD_DIR}" \
        -S "${CURRENT_DIR}/cmake/encaps" \
        -DRUN_MODE="${RUN_MODE}" \
        -DSOC_VERSION="${SOC_VERSION}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}"
    cmake --build "${BUILD_DIR}" -j "${CMAKE_BUILD_JOBS}"
    cmake --install "${BUILD_DIR}"
    echo "${_build_tag}" > "${_build_stamp}"
}

if [ "${_need_build}" = "1" ]; then
    _kem_build
else
    echo "[kem_encaps] skip rebuild (stamp=${_build_tag})"
fi

mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output" "${CURRENT_DIR}/golden"
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${INSTALL_PREFIX}/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
rm -f "${CURRENT_DIR}/ascendc_kem_encaps_bbit"
cp -f "${INSTALL_PREFIX}/bin/ascendc_kem_encaps_bbit" "${CURRENT_DIR}/"

if [ "${RUN_MODE}" = "sim" ]; then
    # 先 sim_env（WSL 装 dump 桩 → out/lib；非 WSL 设 CAMODEL_SKIP_ADX_WORK_PATH），再绑 camodel 日志
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
    echo "[kem_encaps] SIM 2 launch；预算 ${KERNEL_COMPUTE_BUDGET_SEC}s SIM_DIRECT=${SIM_DIRECT}"
fi

# cwd=CURRENT_DIR：kernel-run-timeout 再 source camodel 用 $(pwd)；二进制用相对路径
# 默认直跑（等价于原 kernel-run-timeout.sh）；RUN_WITH_MSPROF=1 时在 sim/npu 下走 msprof op。
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/msprof_run.sh"
msprof_run_kernel ./ascendc_kem_encaps_bbit

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}" || true
fi

if [ "${KEM_ENCAPS_KAT:-0}" = "1" ]; then
    c_sz=$(wc -c <"${CURRENT_DIR}/output/c.bin")
    k_sz=$(wc -c <"${CURRENT_DIR}/output/K.bin")
    if [ "${c_sz}" -ne 1568 ] || [ "${k_sz}" -ne 32 ]; then
        echo "[ERROR] output size c=${c_sz} K=${k_sz}"
        exit 1
    fi
elif [ "${KEM_ENCAPS_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_kem_encaps.py"
fi
if [ "${KEM_ENCAPS_KAT:-0}" != "1" ]; then
    echo "[SUCCESS] stable-fips203-mlkem-kem-encaps-k4 (${RUN_MODE})"
fi
