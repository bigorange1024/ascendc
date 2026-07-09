#!/usr/bin/env bash
# exp-fips203-mlkem-pke-decrypt-k4 — FIPS 203 Alg.15 PKE Decrypt（k=4）
#
# customspec：exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex
# 基线：pass-fix-f203-alg15-pke-decrypt-device-k4（一次性 vendor；本目录自包含）
#
# 生产 I/O：
#   input/  — dk_pke.bin (1536B) + c.bin (1568B) + LUT
#   output/ — m.bin (32B)
#
# Launch：**单 kernel** f203_decrypt_device_fused（GATE flag 4/8 分隔 NTT|INTT）。
# 尾：v−w 向量 mod + Compress₁ 向量 + ByteEncode₁ 标量。
# prep：ByteDecode 标量 + Decompress 向量。
# 生产：仅 D2H m；默认 DECRYPT_GATE=0。
#
# Usage（默认 = 全量）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#
# 调试（非默认）：
#   COMPRESS_1_VEC=0 / DECOMPRESS_D_VEC=0 bash run.sh ...
#   DECRYPT_GATE=4 不可用（生产已取消 mid D2H）
#   bash scripts/vendor_sync.sh   # 仅维护：从活跃探针刷新 vendored 源（非默认）

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
# 生产默认：仅验 m（无中间态 D2H）。调试中间 Gate：DECRYPT_GATE=4（须自行恢复 mid dump，非默认）
export DECRYPT_GATE="${DECRYPT_GATE:-0}"
export DECRYPT_VERIFY="${DECRYPT_VERIFY:-1}"
export KERNEL_COMPUTE_BUDGET_SEC="${DECRYPT_KERNEL_BUDGET_SEC:-600}"

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
# 自包含：compute/ 已 vendored；禁止默认跨探针 vendor_sync（维护见 scripts/vendor_sync.sh）
rm -rf build out
mkdir -p build
cmake -B build \
    -DRUN_MODE="${RUN_MODE}" \
    -DSOC_VERSION="${SOC_VERSION}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}" \
    -DCOMPRESS_1_VEC="${COMPRESS_1_VEC:-1}" \
    -DDECOMPRESS_D_VEC="${DECOMPRESS_D_VEC:-1}"
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

if [ "${DECRYPT_GATE}" != "0" ]; then
    echo "[WARN] DECRYPT_GATE=${DECRYPT_GATE} 需要中间态 bin；生产路径已取消 mid D2H，Gate 将失败。请用 DECRYPT_GATE=0。" >&2
    DECRYPT_GATE="${DECRYPT_GATE}" python3 "${CURRENT_DIR}/scripts/verify_gate.py"
fi

if [ "${DECRYPT_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_result.py"
fi

echo "[SUCCESS] exp-fips203-mlkem-pke-decrypt-k4 (${RUN_MODE}) DECRYPT_VERIFY=${DECRYPT_VERIFY} COMPRESS_1_VEC=${COMPRESS_1_VEC:-1} DECOMPRESS_D_VEC=${DECOMPRESS_D_VEC:-1}"
