#!/usr/bin/env bash
# fix-f203-alg14-pke-encrypt-correctness-k4 — FIPS 203 Alg.14 PKE Encrypt（设备拼装探针）
#
# 生产 I/O：
#   input/  — ek_pke.bin (1568B) + m.bin (32B) + coins.bin (32B)
#   output/ — c.bin (1568B) 占位；G1 另写 a_hat/r/e1/e2
#
# G5（当前默认）：单 session 全链；设备 ByteDecode ek→t̂；无 input/t_hat.bin
# G4：G3 + INTT + 噪声/μ + Compress/ByteEncode → c.bin 1568B（staging t_hat.bin）
# G0：ENCRYPT_GATE=0 → marker 壳 only
# 字节对拍：ENCRYPT_VERIFY=1（须 host_golden/golden_c.py，G4 后启用）
#
# Usage（默认）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#
# 调试（非默认）：
#   ENCRYPT_GATE=0 bash run.sh -r cpu -v Ascend910B4
#   ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4

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
export ENCRYPT_VERIFY="${ENCRYPT_VERIFY:-0}"
# tail-only 快跑（仅 sim）：跳过 prep/NTT/G3，读 golden u_hat/tr_hat/e1/e2 只跑 INTT→noise→pack，
# 始终分阶段 dump，配 verify_g4_tail.py 逐阶段对拍定位首个对不上的阶段。
export ENCRYPT_TAIL_ONLY="${ENCRYPT_TAIL_ONLY:-0}"
# 全链 SIM 调试：设为 1 时全链 tail 也逐阶段 dump（默认 0）。
export ENCRYPT_DUMP_TAIL="${ENCRYPT_DUMP_TAIL:-0}"
export KERNEL_COMPUTE_BUDGET_SEC="${ENCRYPT_KERNEL_BUDGET_SEC:-600}"

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

# tail-only：在跑 kernel 前生成 golden 中间量（含设备读取的 golden_{u_hat,tr_hat,e1,e2}.bin）
if [ "${ENCRYPT_TAIL_ONLY}" != "0" ]; then
    python3 "${CURRENT_DIR}/scripts/host_golden/gen_g4_tail_golden.py" "${CURRENT_DIR}" "${CURRENT_DIR}/output"
fi

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

if [ "${ENCRYPT_TAIL_ONLY}" != "0" ]; then
    # tail-only：只做分阶段对拍（不依赖设备 a_hat/r_hat，故不跑 verify_gate）
    python3 "${CURRENT_DIR}/scripts/verify_g4_tail.py"
    echo "[SUCCESS] fix-f203-alg14-pke-encrypt-correctness-k4 TAIL-ONLY (${RUN_MODE})"
    exit 0
fi

if [ "${ENCRYPT_GATE}" != "0" ]; then
    ENCRYPT_GATE="${ENCRYPT_GATE}" python3 "${CURRENT_DIR}/scripts/verify_gate.py"
fi

if [ "${ENCRYPT_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_result.py"
fi
echo "[SUCCESS] fix-f203-alg14-pke-encrypt-correctness-k4 gate=G${ENCRYPT_GATE} (${RUN_MODE}) ENCRYPT_VERIFY=${ENCRYPT_VERIFY}"
