#!/usr/bin/env bash
# fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4 — 行 18–19 可行性（单 MIX launch）
#
# Usage:
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   F203_FEAS_FUSED=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # 单 launch 试验

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"

BUILD_TYPE="Debug"
INSTALL_PREFIX="${CURRENT_DIR}/out"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"

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
    *) echo "[ERROR] Unexpected option: $1"; exit 1 ;;
    esac
done

if [ -n "${ASCEND_INSTALL_PATH:-}" ]; then
    _ASCEND_INSTALL_PATH="${ASCEND_INSTALL_PATH}"
elif [ -n "${ASCEND_HOME_PATH:-}" ]; then
    _ASCEND_INSTALL_PATH="${ASCEND_HOME_PATH}"
elif [ -d "$HOME/Ascend/ascend-toolkit/latest" ]; then
    _ASCEND_INSTALL_PATH="$HOME/Ascend/ascend-toolkit/latest"
elif [ -d "$HOME/Ascend/cann" ]; then
    _ASCEND_INSTALL_PATH="$HOME/Ascend/cann"
else
    _ASCEND_INSTALL_PATH=/usr/local/Ascend/ascend-toolkit/latest
fi

export ASCEND_TOOLKIT_HOME="${_ASCEND_INSTALL_PATH}"
export ASCEND_HOME_PATH="${_ASCEND_INSTALL_PATH}"

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -f "${_ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    # shellcheck source=/dev/null
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
fi

set -e
rm -rf build out
mkdir -p build
cmake -B build \
    -DRUN_MODE="${RUN_MODE}" \
    -DSOC_VERSION="${SOC_VERSION}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}" \
    -DF203_STAGE1_SPLIT="${F203_STAGE1_SPLIT:-1}" \
    -DF203_STAGE3_MOD="${F203_STAGE3_MOD:-0}"
cmake --build build -j
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH="$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"

if [ "${RUN_MODE}" = "sim" ]; then
    if [ -n "${F203_FEAS_FUSED:-}" ]; then
        export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-600}"
        echo "[run.sh] F203_FEAS_FUSED=1 单 launch；预算 ${KERNEL_COMPUTE_BUDGET_SEC}s（host 每 5s 轮询 fused-trace）"
    else
        export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-180}"
    fi
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
fi

bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

python3 - <<'PY'
import numpy as np
import sys

def max_diff(a, b):
    return int(np.max(np.abs(a.astype(np.int64) - b.astype(np.int64))))

y_hat = np.fromfile("output/y_hat.bin", dtype=np.int32).reshape(4, 256)
u_ntt = np.fromfile("output/u_ntt.bin", dtype=np.int32).reshape(4, 256)
u = np.fromfile("output/u.bin", dtype=np.int32).reshape(4, 256)
gy = np.fromfile("output/golden_y_hat.bin", dtype=np.int32).reshape(4, 256)
gu = np.fromfile("output/golden_u_ntt.bin", dtype=np.int32).reshape(4, 256)
gu_out = np.fromfile("output/golden_u.bin", dtype=np.int32).reshape(4, 256)

dy = max_diff(y_hat, gy)
du = max_diff(u_ntt, gu)
duo = max_diff(u, gu_out)
print(f"[cmp] y_hat max={dy} u_ntt max={du} u max={duo}")
if dy or du or duo:
    sys.exit(1)
print("[SUCCESS] line18-19 feasibility: y_hat + u_ntt + u match golden")
PY

echo "[SUCCESS] fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4 (${RUN_MODE})"
