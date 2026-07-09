#!/usr/bin/env bash
# pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4 — Alg.14 行 2/16–24 **PASS** 探针
#
# SIM：**1 launch**（f203_encrypt_l18_l19：e₂+=μ + compute + 内联 tail pack → c）
# CPU：compute 三 launch 产 u；v=golden；独立 pack launch → c
# tail：Compress 向量 + ByteEncode 标量 pack（不启用 BYTE_ENCODE_D_VEC=2）
# 定稿：docs/notes/F203-Alg14-Encrypt-compute-tail-PASS技术总结.md
#
# Usage（默认 = 全量生产路径；无需手动 export SIM_DIRECT）:
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4          # run.sh 在 sim 模式内自动 export SIM_DIRECT=1

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
    # 默认 = CAModel 金标路径；勿要求用户手动 SIM_DIRECT=1
    export SIM_DIRECT="${SIM_DIRECT:-1}"
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
rm -rf input output golden
mkdir -p input output golden
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH="$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"

if [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-720}"
    echo "[run.sh] SIM 2 launch；预算 ${KERNEL_COMPUTE_BUDGET_SEC}s"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
else
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-600}"
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

def max_diff_bytes(a: bytes, b: bytes) -> int:
    if len(a) != len(b):
        return 10**9
    x = np.frombuffer(a, dtype=np.uint8).astype(np.int16)
    y = np.frombuffer(b, dtype=np.uint8).astype(np.int16)
    return int(np.max(np.abs(x - y)))

def max_diff_i32(path_a, path_b, shape):
    a = np.fromfile(path_a, dtype=np.int32).reshape(shape)
    b = np.fromfile(path_b, dtype=np.int32).reshape(shape)
    return int(np.max(np.abs(a.astype(np.int64) - b.astype(np.int64))))

run_mode = os.environ.get("ASCENDC_RUN_MODE_CMP", "sim")
dc = max_diff_bytes(open("output/c.bin", "rb").read(), open("golden/c.bin", "rb").read())
du = max_diff_i32("output/u.bin", "output/golden_u.bin", (4, 256))
dv = max_diff_i32("output/v.bin", "output/golden_v.bin", (256,))
print(f"[cmp] c max={dc} u max={du} v max={dv}")
# c/u 两模式均为设备产出，须逐字节对齐 golden；v 仅 SIM 融合核产出（CPU 由 golden 补）
if dc or du:
    sys.exit(1)
if run_mode == "sim" and dv:
    sys.exit(1)
print("[SUCCESS] compute+tail: c + u" + (" + v" if run_mode == "sim" else "") + " match golden")
PY

echo "[SUCCESS] pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4 (${RUN_MODE})"
