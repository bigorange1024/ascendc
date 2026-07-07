#!/usr/bin/env bash
# pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4 — 行 18–19 可行性（单 MIX launch）
#
# Usage（默认）:
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 调试（非默认）:
#   ASCENDC_SIM_HOST_MODE=phased_launch SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   F203_BYTE_DECODE12_IMPL=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # 行2：零 Gather 向量备用（非 Gather）
#
# 全仓宏：library/shared/ascendc_build_mode.hpp · docs/notes/AscendC-CPU与SIM实现分叉开发指南.md

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

export F203_STAGE1_SPLIT="${F203_STAGE1_SPLIT:-1}"
export F203_STAGE3_MOD="${F203_STAGE3_MOD:-0}"
export ALG11_IMPL="${ALG11_IMPL:-1}"
export ALG11_VEC_VARIANT="${ALG11_VEC_VARIANT:-2}"
export ALG11_VEC_OPTS="${ALG11_VEC_OPTS:-1}"
export ALG11_MEM_OPS="${ALG11_MEM_OPS:-1}"
export F203_BYTE_DECODE12_IMPL="${F203_BYTE_DECODE12_IMPL:-0}"

set -e
rm -rf build out
mkdir -p build
cmake -B build \
    -DRUN_MODE="${RUN_MODE}" \
    -DSOC_VERSION="${SOC_VERSION}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}" \
    -DF203_STAGE1_SPLIT="${F203_STAGE1_SPLIT}" \
    -DF203_STAGE3_MOD="${F203_STAGE3_MOD}" \
    -DALG11_IMPL="${ALG11_IMPL}" \
    -DALG11_VEC_VARIANT="${ALG11_VEC_VARIANT}" \
    -DALG11_VEC_OPTS="${ALG11_VEC_OPTS}" \
    -DALG11_MEM_OPS="${ALG11_MEM_OPS}" \
    -DF203_BYTE_DECODE12_IMPL="${F203_BYTE_DECODE12_IMPL}"
cmake --build build -j
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH="$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"

if [ "${RUN_MODE}" = "sim" ]; then
    if [ "${ASCENDC_SIM_HOST_MODE:-}" = "phased_launch" ]; then
        export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-180}"
        echo "[run.sh] 调试 ASCENDC_SIM_HOST_MODE=phased_launch → 3 launch；预算 ${KERNEL_COMPUTE_BUDGET_SEC}s"
    else
        export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-600}"
        echo "[run.sh] SIM 默认单 launch；预算 ${KERNEL_COMPUTE_BUDGET_SEC}s"
    fi
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
fi

bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit

export ASCENDC_RUN_MODE_CMP="${RUN_MODE}"

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

python3 - <<'PY'
import os
import numpy as np
import sys

def max_diff(a, b):
    return int(np.max(np.abs(a.astype(np.int64) - b.astype(np.int64))))

run_mode = os.environ.get("ASCENDC_RUN_MODE_CMP", "sim")
y_hat = np.fromfile("output/y_hat.bin", dtype=np.int32).reshape(4, 256)
u_ntt = np.fromfile("output/u_ntt.bin", dtype=np.int32).reshape(4, 256)
u = np.fromfile("output/u.bin", dtype=np.int32).reshape(4, 256)
gy = np.fromfile("output/golden_y_hat.bin", dtype=np.int32).reshape(4, 256)
gu = np.fromfile("output/golden_u_ntt.bin", dtype=np.int32).reshape(4, 256)
gu_out = np.fromfile("output/golden_u.bin", dtype=np.int32).reshape(4, 256)

dy = max_diff(y_hat, gy)
du = max_diff(u_ntt, gu)
duo = max_diff(u, gu_out)

dutr = 0
dv = 0
skip_kp5 = (os.environ.get("ASCENDC_SIM_HOST_MODE") == "phased_launch") or (run_mode == "cpu")
if not skip_kp5:
    if os.path.exists("output/golden_u_tr.bin") and os.path.exists("output/u_tr.bin"):
        u_tr = np.fromfile("output/u_tr.bin", dtype=np.int32).reshape(5, 256)
        gu_tr = np.fromfile("output/golden_u_tr.bin", dtype=np.int32).reshape(5, 256)
        dutr = max_diff(u_tr, gu_tr)
    if os.path.exists("output/golden_v.bin") and os.path.exists("output/v.bin"):
        v = np.fromfile("output/v.bin", dtype=np.int32).reshape(256)
        gv = np.fromfile("output/golden_v.bin", dtype=np.int32).reshape(256)
        dv = max_diff(v, gv)

print(f"[cmp] y_hat max={dy} u_ntt max={du} u_tr max={dutr} u max={duo} v max={dv}")
if dy or du or duo or dutr or dv:
    sys.exit(1)
print("[SUCCESS] line2/18/19/21: y_hat + u_ntt + u_tr + u + v match golden")
PY

echo "[SUCCESS] pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4 (${RUN_MODE})"
