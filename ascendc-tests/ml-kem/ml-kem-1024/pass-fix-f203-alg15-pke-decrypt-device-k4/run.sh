#!/usr/bin/env bash
# pass-fix-f203-alg15-pke-decrypt-device-k4 — FIPS 203 Alg.15 PKE Decrypt（优化）
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
#
# 多环境分流（scripts/runtime_env.sh）：
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#   bash run.sh -r npu -v Ascend910B4       # 真机；未设 ASCEND_DEVICE_ID 时 tests→3（SIM 强制 0）
#   WSL 禁止 -r npu；说明见 docs/engineering/NPU真机环境说明.md
#
# msprof 性能采集（须显式指定，非默认；默认不产生任何 prof/OPPROF 文件）:
#   RUN_WITH_MSPROF=1 bash run.sh -r npu -v Ascend910B4   # 实机 msprof op → prof_npu/<bin>/
#   开启后墙钟由 msprof 报告给出，output/run_metrics.txt 不再是 /usr/bin/time 的 [wall_sec]
# CANN：source ${REPO_ROOT}/scripts/env.sh（勿写死 ~/ascendc）

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
_ORIG_ARGS=("$@")
_REPO_CAND="$(cd "${CURRENT_DIR}/../.." && pwd)"
if [ -d "${_REPO_CAND}/library/shared" ]; then
    REPO_ROOT="${_REPO_CAND}"
else
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


# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
runtime_env_dispatch "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"

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

# 实机 ACL 设备号：npu 按树分卡（见 npu_device_map.sh）；SIM 强制 0
if [ "${RUN_MODE}" = "npu" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/npu_device_map.sh"
    npu_device_map_apply "${CURRENT_DIR}"
elif [ "${RUN_MODE}" = "sim" ]; then
    export ASCEND_DEVICE_ID=0
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-900}"
    export SIM_DIRECT="${SIM_DIRECT:-1}"
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "npu" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-600}"
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
    echo "[npu] ASCEND_DEVICE_ID=${ASCEND_DEVICE_ID}  CANN=${_ASCEND_INSTALL_PATH}"
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

# 默认：msprof_run_kernel（与 stable 一致；RUN_WITH_MSPROF=1 走 msprof op）
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/msprof_run.sh"
msprof_run_kernel ./ascendc_kernels_bbit

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

echo "[SUCCESS] pass-fix-f203-alg15-pke-decrypt-device-k4 (${RUN_MODE}) DECRYPT_VERIFY=${DECRYPT_VERIFY} COMPRESS_1_VEC=${COMPRESS_1_VEC:-1} DECOMPRESS_D_VEC=${DECOMPRESS_D_VEC:-1}"
