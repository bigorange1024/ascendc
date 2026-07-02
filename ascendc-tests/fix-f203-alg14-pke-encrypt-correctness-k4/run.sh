#!/usr/bin/env bash
# fix-f203-alg14-pke-encrypt-correctness-k4 — FIPS 203 Alg.14 PKE Encrypt（设备拼装探针）
#
# 生产 I/O：
#   input/  — ek_pke.bin (1568B) + m.bin (32B) + coins.bin (32B)
#   output/ — c.bin (1568B)（唯一算法输出；中间张量不落盘）
#
# G5（唯一生产路径，默认 ENCRYPT_GATE=5）：单 session 全 device 链；无 input/t_hat.bin
# 验收（默认）：c.bin 与 golden 对拍（verify_result.py）
#
# G0–G4：过渡路线，G5 测通后已冻结（frozen-gates/FROZEN.md）；每测通下一 Gate 即冻结上一 Gate
#
# Usage（默认，无需手动 export）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#
# 调试（非默认）：
#   ENCRYPT_VERIFY=0 bash run.sh -r cpu -v Ascend910B4   # 跳过 c.bin 对拍
#   ENCRYPT_GATE=0..4 bash run.sh …                      # 过渡回放（已冻结，会 WARN）

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
_REPO_CAND="$(cd "${CURRENT_DIR}/../.." && pwd)"
if [ -d "${_REPO_CAND}/library/shared" ]; then
    REPO_ROOT="${_REPO_CAND}"
else
    REPO_ROOT="$(cd "${CURRENT_DIR}/../../.." && pwd)"
fi

BUILD_TYPE="Debug"
INSTALL_PREFIX="${CURRENT_DIR}/out"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"
export SEED_D="${SEED_D:-20260619}"
export ENCRYPT_GATE="${ENCRYPT_GATE:-5}"
export ENCRYPT_VERIFY="${ENCRYPT_VERIFY:-1}"
export KERNEL_COMPUTE_BUDGET_SEC="${ENCRYPT_KERNEL_BUDGET_SEC:-600}"

if [ "${ENCRYPT_GATE}" != "5" ]; then
    echo "[WARN] ENCRYPT_GATE=${ENCRYPT_GATE}：过渡路线已冻结；生产验收请用默认 G5（见 frozen-gates/FROZEN.md）" >&2
fi

SHORT=r:,v:,i:,b:,p:
LONG=run-mode:,soc-version:,install-path:,build-type:,install-prefix:
OPTS=$(getopt -a --options $SHORT --longoptions $LONG -- "$@")
eval set -- "$OPTS"
while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    -i | --install-path) ASCEND_INSTALL_PATH="$2"; shift 2 ;;
    -b | --build-type) BUILD_TYPE="$2"; shift 2 ;;
    -p | --install-prefix) INSTALL_PREFIX="$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[ERROR] option $1"; exit 1 ;;
    esac
done

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -n "${ASCEND_INSTALL_PATH:-}" ] && [ -f "${ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    _ASCEND_INSTALL_PATH="${ASCEND_INSTALL_PATH}"
elif [ -d "$HOME/Ascend/ascend-toolkit/latest" ]; then
    _ASCEND_INSTALL_PATH="$HOME/Ascend/ascend-toolkit/latest"
else
    _ASCEND_INSTALL_PATH=/usr/local/Ascend/ascend-toolkit/latest
fi
export ASCEND_TOOLKIT_HOME="${_ASCEND_INSTALL_PATH}"
export ASCEND_HOME_PATH="${_ASCEND_INSTALL_PATH}"

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT=1
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
fi

set -e
bash "${CURRENT_DIR}/scripts/vendor_sync.sh"
rm -rf build out
mkdir -p build
cmake -B build \
    -DRUN_MODE="${RUN_MODE}" \
    -DSOC_VERSION="${SOC_VERSION}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}"
cmake --build build -j
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH="$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"

if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi

/usr/bin/time -f '[wall_sec] %e' bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit 2>&1 | tee "${CURRENT_DIR}/output/run_metrics.txt"

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

if [ "${ENCRYPT_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_result.py"
fi
echo "[SUCCESS] fix-f203-alg14-pke-encrypt-correctness-k4 gate=G${ENCRYPT_GATE} (${RUN_MODE}) ENCRYPT_VERIFY=${ENCRYPT_VERIFY}"
