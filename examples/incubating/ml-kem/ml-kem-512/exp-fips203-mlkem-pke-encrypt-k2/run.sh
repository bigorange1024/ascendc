#!/usr/bin/env bash
# exp-fips203-mlkem-pke-encrypt-k2 — 完整 K-PKE.Encrypt（FIPS 203 Alg.14 行 1–22）
#
# prep（ek+coins→a_hat+re）+ compute+tail（→c）单 device session GM handoff：
#   SIM：2 launch（prep → l18_l19 含 e₂+=μ 与内联 tail pack）
#   CPU：5 launch（prep + ntt_y/at_jp/intt_e1 + pack；v=golden_v）
# golden：SEED_D=20260619；优先复用 correctness 产物，缺失时本目录自生成（见 scripts/gen_data.py）
#
# Usage（默认 = 全量生产路径；无需手动 export SIM_DIRECT / HAT_*）:
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4          # run.sh 在 sim 模式内自动 export SIM_DIRECT=1
#
# 默认行为（对齐 alg20）:
#   ENCRYPT_SKIP_REBUILD=1   — 二进制与 RUN_MODE stamp 在则跳过 cmake
#   CMAKE_BUILD_JOBS=2       — 限并行，WSL 友好
#
# 调试（须显式指定，非默认）:
#   ENCRYPT_FORCE_REBUILD=1 bash run.sh -r cpu -v Ascend910B4   # 强制 rm build/out 全量重编
#   SIM_DIRECT=0 bash run.sh -r sim -v Ascend910B4              # 走 msprof + OPPROF_*（慢）
#   改 F203_* / ALG11_* 编译开关后须 FORCE_REBUILD=1（stamp 含主要宏）
#
# 多环境分流（scripts/runtime_env.sh）：
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#   WSL 禁止 -r npu；说明见 docs/engineering/NPU真机环境说明.md

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

BUILD_TYPE="Debug"
INSTALL_PREFIX="${CURRENT_DIR}/out"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"

export ENCRYPT_SKIP_REBUILD="${ENCRYPT_SKIP_REBUILD:-1}"
export ENCRYPT_FORCE_REBUILD="${ENCRYPT_FORCE_REBUILD:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"

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


# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
runtime_env_dispatch "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"
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
    # 默认 = CAModel 金标路径（与 keygen/encaps 一致）；勿要求用户手动 SIM_DIRECT=1
    export SIM_DIRECT="${SIM_DIRECT:-1}"
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
fi

# 生产默认（compute 全量向量 + prep 全量路径）；调试分段须显式覆盖对应 env
export F203_STAGE1_SPLIT="${F203_STAGE1_SPLIT:-1}"
export F203_STAGE3_MOD="${F203_STAGE3_MOD:-0}"
export ALG11_IMPL="${ALG11_IMPL:-1}"
export ALG11_VEC_VARIANT="${ALG11_VEC_VARIANT:-2}"
export ALG11_VEC_OPTS="${ALG11_VEC_OPTS:-1}"
export ALG11_MEM_OPS="${ALG11_MEM_OPS:-1}"
export F203_BYTE_DECODE12_IMPL="${F203_BYTE_DECODE12_IMPL:-0}"
export F203_ALG7_REJ_IMPL="${F203_ALG7_REJ_IMPL:-1}"
export F203_ALG7_D12_GATHER="${F203_ALG7_D12_GATHER:-0}"
export F203_AHAT16_BLOCK_DIM="${F203_AHAT16_BLOCK_DIM:-2}"
export F203_AHAT16_BATCH_SHAKE="${F203_AHAT16_BATCH_SHAKE:-0}"
export F203_ALG7_XOF_504="${F203_ALG7_XOF_504:-0}"
export F203_CBD_BLOCK_DIM="${F203_CBD_BLOCK_DIM:-1}"

BUILD_DIR="${CURRENT_DIR}/build"
_build_stamp="${BUILD_DIR}/.encrypt_full_run_mode"
_build_tag="${RUN_MODE}:s1=${F203_STAGE1_SPLIT}:s3=${F203_STAGE3_MOD}:a11=${ALG11_IMPL}:v=${ALG11_VEC_VARIANT}:bd12=${F203_BYTE_DECODE12_IMPL}:ahat=${F203_AHAT16_BLOCK_DIM}:cbd=${F203_CBD_BLOCK_DIM}"

_need_build=1
if [ "${ENCRYPT_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${ENCRYPT_SKIP_REBUILD}" = "1" ] && [ -x "${INSTALL_PREFIX}/bin/ascendc_kernels_bbit" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_encrypt_build() {
    if [ "${ENCRYPT_FORCE_REBUILD}" = "1" ] || \
       { [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" != "${_build_tag}" ]; }; then
        rm -rf "${BUILD_DIR}" "${INSTALL_PREFIX}"
    fi
    mkdir -p "${BUILD_DIR}"
    cmake -S "${CURRENT_DIR}" -B "${BUILD_DIR}" \
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
        -DF203_BYTE_DECODE12_IMPL="${F203_BYTE_DECODE12_IMPL}" \
        -DF203_ALG7_REJ_IMPL="${F203_ALG7_REJ_IMPL}" \
        -DF203_ALG7_D12_GATHER="${F203_ALG7_D12_GATHER}" \
        -DF203_AHAT16_BLOCK_DIM="${F203_AHAT16_BLOCK_DIM}" \
        -DF203_AHAT16_BATCH_SHAKE="${F203_AHAT16_BATCH_SHAKE}" \
        -DF203_ALG7_XOF_504="${F203_ALG7_XOF_504}" \
        -DF203_CBD_BLOCK_DIM="${F203_CBD_BLOCK_DIM}"
    cmake --build "${BUILD_DIR}" -j"${CMAKE_BUILD_JOBS}"
    cmake --install "${BUILD_DIR}"
    echo "${_build_tag}" >"${_build_stamp}"
}

set -e
if [ "${_need_build}" = "1" ]; then
    echo "[run.sh] build RUN_MODE=${RUN_MODE} jobs=${CMAKE_BUILD_JOBS}"
    _encrypt_build
else
    echo "[run.sh] skip rebuild (RUN_MODE=${RUN_MODE}, stamp OK)"
fi

rm -f "${CURRENT_DIR}/ascendc_kernels_bbit"
cp -f "${INSTALL_PREFIX}/bin/ascendc_kernels_bbit" "${CURRENT_DIR}/"
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output" "${CURRENT_DIR}/golden"
# 每次刷新 golden（确定性、秒级）；不删已有 input 中非本脚本产物
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${INSTALL_PREFIX}/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"

if [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-900}"
    echo "[run.sh] SIM 2 launch（prep + l18_l19）；预算 ${KERNEL_COMPUTE_BUDGET_SEC}s SIM_DIRECT=${SIM_DIRECT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
else
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-600}"
fi

bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

python3 - <<'PY'
import sys

import numpy as np


def max_diff_bytes(a: bytes, b: bytes) -> int:
    if len(a) != len(b):
        return 10**9
    x = np.frombuffer(a, dtype=np.uint8).astype(np.int16)
    y = np.frombuffer(b, dtype=np.uint8).astype(np.int16)
    return int(np.max(np.abs(x - y)))


# 验收：Alg.14 唯一输出密文 c（两模式均为设备产出，须逐字节对齐 golden）
dc = max_diff_bytes(open("output/c.bin", "rb").read(), open("golden/c.bin", "rb").read())
print(f"[cmp] c max={dc}")
if dc:
    sys.exit(1)
print("[SUCCESS] full encrypt: c matches golden")
PY

echo "[SUCCESS] exp-fips203-mlkem-pke-encrypt-k2 (${RUN_MODE})"
