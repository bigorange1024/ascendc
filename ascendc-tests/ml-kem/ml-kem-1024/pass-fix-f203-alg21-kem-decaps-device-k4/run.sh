#!/usr/bin/env bash
# pass-fix-f203-alg21-kem-decaps-device-k4 — Alg.21 Decaps 全链（D+E）；可选 Phase-E-only
#
# 生产 I/O：input/{dk_kem,c,lut_*} → output/K.bin
# PKE：编译期引用 stable Decrypt + Encrypt；见 INTEGRATION_PLAN.md
#
# Usage（默认全链）:
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4   # 默认单库 + decaps_1session；T19i 后 SIM 3 launch（pack+FO 同核）
#
# 调试（非默认）:
#   KEM_DECAPS_PHASEE_ONLY=1 …          # 仅 Phase-E（灌 m'）
#   KEM_DECAPS_REJECT=1 …               # Gate E3：随机假密文；K vs liboqs Decaps≡J(z‖c)
#   ASCENDC_SIM_HOST_MODE=decaps_2session …  # SIM 双 session 对照（非默认）
#   KEM_DECAPS_FORCE_REBUILD=1 …
#
# msprof 性能采集（须显式指定，非默认；默认不产生任何 prof/OPPROF 文件）:
#   RUN_WITH_MSPROF=1 bash run.sh -r npu -v Ascend910B4   # 实机 msprof op → prof_npu/<bin>/（D/E 各一份）
#   可调项见 scripts/msprof_run.sh

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
_ORIG_ARGS=("$@")
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

mkdir -p "${CURRENT_DIR}/scripts"
STABLE_ENCRYPT="${REPO_ROOT}/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4"
# -r：生成**相对**软链，换机（实机 / Cloud / WSL 仓库路径各不相同）后无需重写，git 工作区也不会被污染
ln -sfnr "${STABLE_ENCRYPT}/scripts/host_golden" "${CURRENT_DIR}/scripts/host_golden"
ln -sfnr "${STABLE_ENCRYPT}/thirdparty" "${CURRENT_DIR}/thirdparty"

export KEM_DECAPS_VERIFY="${KEM_DECAPS_VERIFY:-1}"
export KEM_DECAPS_SKIP_REBUILD="${KEM_DECAPS_SKIP_REBUILD:-${KEM_SKIP_REBUILD:-1}}"
export KEM_DECAPS_FORCE_REBUILD="${KEM_DECAPS_FORCE_REBUILD:-0}"
export KEM_DECAPS_PHASEE_ONLY="${KEM_DECAPS_PHASEE_ONLY:-0}"
export KEM_DECAPS_REJECT="${KEM_DECAPS_REJECT:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEM_DECAPS_KERNEL_BUDGET_SEC:-1200}"
# SIM 全链默认 1-session（T2 单库后）；对照可显式 decaps_2session
if [ -z "${ASCENDC_SIM_HOST_MODE:-}" ]; then
    export ASCENDC_SIM_HOST_MODE=decaps_1session
fi
# 验收注意：verify 失败须非零退出（已 || exit $?）；KAT/roundtrip 合法路径须传 M_FILE；勿并行多路同目录 SIM。

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

if [ "${KEM_DECAPS_PHASEE_ONLY}" = "1" ]; then
    BUILD_PROFILE="${KEM_DECAPS_BUILD_PROFILE:-phase_e}"
    BIN_NAME="ascendc_kem_decaps_phase_e_bbit"
    GEN_SCRIPT="gen_data_phase_e.py"
    LABEL="Phase-E-only"
elif [ "${KEM_DECAPS_REJECT}" = "1" ]; then
    BUILD_PROFILE="${KEM_DECAPS_BUILD_PROFILE:-prod}"
    BIN_NAME="ascendc_kem_decaps_bbit"
    GEN_SCRIPT="gen_data.py"
    LABEL="REJECT(E3)"
else
    BUILD_PROFILE="${KEM_DECAPS_BUILD_PROFILE:-prod}"
    BIN_NAME="ascendc_kem_decaps_bbit"
    GEN_SCRIPT="gen_data.py"
    LABEL="full D+E"
fi

BUILD_DIR="${CURRENT_DIR}/build_${BUILD_PROFILE}_${RUN_MODE}"
if [ -z "${INSTALL_PREFIX}" ]; then
    INSTALL_PREFIX="${CURRENT_DIR}/out_${BUILD_PROFILE}_${RUN_MODE}"
fi

# CANN：统一 ${REPO_ROOT}/scripts/env.sh（多候选 + 回写 CANN_HOME）；禁止写死 ~/ascendc
set +e
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/env.sh"
_env_rc=$?
set -e
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

set -euo pipefail

# 实机 ACL 设备号：npu 缺省 1；SIM 强制 0
if [ "${RUN_MODE}" = "npu" ]; then
    export ASCEND_DEVICE_ID="${ASCEND_DEVICE_ID:-1}"
elif [ "${RUN_MODE}" = "sim" ]; then
    export ASCEND_DEVICE_ID=0
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT="${SIM_DIRECT:-1}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "npu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
    echo "[npu] ASCEND_DEVICE_ID=${ASCEND_DEVICE_ID}  CANN=${_ASCEND_INSTALL_PATH}"
fi

echo "[kem_decaps ${LABEL}] RUN_MODE=${RUN_MODE} profile=${BUILD_PROFILE} SIM_HOST_MODE=${ASCENDC_SIM_HOST_MODE:-} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC}"

_build_stamp="${BUILD_DIR}/.kem_decaps_run_mode"
_build_tag="${BUILD_PROFILE}:${RUN_MODE}"
_need_build=1
if [ "${KEM_DECAPS_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${KEM_DECAPS_SKIP_REBUILD}" = "1" ] && [ -x "${INSTALL_PREFIX}/bin/${BIN_NAME}" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ ! -f "${INSTALL_PREFIX}/lib/libascendc_kernels_dec_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_kem_build() {
    if [ "${KEM_DECAPS_FORCE_REBUILD}" = "1" ] || \
       { [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" != "${_build_tag}" ]; }; then
        rm -rf "${BUILD_DIR}" "${INSTALL_PREFIX}"
    fi
    # SIM/NPU：合库前生成 Decrypt 冲突头隔离 shim（幂等；不改 stable）
    if [ "${RUN_MODE}" = "sim" ] || [ "${RUN_MODE}" = "npu" ]; then
        bash "${CURRENT_DIR}/scripts/prepare_dec_shim.sh"
    fi
    mkdir -p "${BUILD_DIR}"
    cmake -B "${BUILD_DIR}" \
        -S "${CURRENT_DIR}/cmake/decaps" \
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
    echo "[kem_decaps] skip rebuild (stamp=${_build_tag})"
fi

python3 "${CURRENT_DIR}/scripts/${GEN_SCRIPT}"

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${INSTALL_PREFIX}/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
rm -f "${CURRENT_DIR}/${BIN_NAME}"
cp -f "${INSTALL_PREFIX}/bin/${BIN_NAME}" "${CURRENT_DIR}/"

if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi

# 默认直跑（等价 kernel-run-timeout.sh）；仅 RUN_WITH_MSPROF=1 时在 sim/npu 下走 msprof op。
# 采集目录按二进制名分开，D/E 两个 phase 各自留档。
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/msprof_run.sh"
msprof_run_kernel "${CURRENT_DIR}/${BIN_NAME}"

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}" || true
fi

if [ "${KEM_DECAPS_VERIFY}" = "1" ]; then
    # 对拍失败须非零退出，禁止假 SUCCESS（曾掩盖并发污染下的 FAIL）
    python3 "${CURRENT_DIR}/scripts/verify_kem_decaps.py" || exit $?
fi
echo "[SUCCESS] pass-fix-f203-alg21-kem-decaps-device-k4 ${LABEL} (${RUN_MODE})"
