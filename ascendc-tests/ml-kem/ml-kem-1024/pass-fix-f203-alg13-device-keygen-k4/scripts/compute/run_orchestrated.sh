#!/bin/bash
# @probe pass-fix-f203-alg13-device-keygen-k4
# @file scripts/compute/run_orchestrated.sh
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `run_orchestrated.sh`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps. compute 子树可单独跑中间 bin（调试）。 / Compute subtree debug bins optional.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

# G4 compute 段：本目录 compute/ 内核，mixPass=0 单 launch（选项已锁定）。
set -e
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
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
COMPUTE_IO="${CURRENT_DIR}/compute_io"
OUT_INSTALL="${CURRENT_DIR}/out_compute"
BUILD_DIR="${CURRENT_DIR}/build_compute"

RUN_MODE="${1:-cpu}"
SOC_VERSION="${2:-Ascend910B4}"
BUILD_TYPE="Debug"

export NTTS2S1E_MIX_PASS=0
export F203_KEYGEN_EK_PKE=1
export KEYGEN_ORCHESTRATE=1
export F203_STAGE1_SPLIT=1
export HAT_LINE18_DOT_ONLY=0
export HAT_BYTE_ENCODE=1
export F203_PIPELINE_PROBE=0
export F203_PROBE_EARLY=0
export HAT_ALG11_VEC=1
export BYTE_ENCODE12_VEC=1
export BYTE_ENCODE12_SCATTER_VEC=1
export BYTE_ENCODE12_PREFETCH=1
export ALG11_IMPL=1
export ALG11_VEC_VARIANT=2
export ALG11_VEC_OPTS=1
export ALG11_MEM_OPS=1

if [ -f "${REPO_ROOT}/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/env.sh"
    _ASCEND="${CANN_HOME}"
elif [ -n "${ASCEND_INSTALL_PATH:-}" ] && [ -f "${ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    _ASCEND="${ASCEND_INSTALL_PATH}"
    # shellcheck source=/dev/null
    source "${_ASCEND}/bin/setenv.bash"
else
    _ASCEND="${ASCEND_TOOLKIT_HOME:-/usr/local/Ascend/ascend-toolkit/latest}"
fi
if [ "${RUN_MODE}" = "npu" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/npu_device_map.sh"
    npu_device_map_apply "${CURRENT_DIR}"
elif [ "${RUN_MODE}" = "sim" ]; then
    export ASCEND_DEVICE_ID=0
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT=1
    export LD_LIBRARY_PATH="${_ASCEND}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-900}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # compute 段独立 sim_log，避免与 prep 段共用 ASCEND_WORK_PATH 导致 FPE/串台
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${COMPUTE_IO}"
else
    export LD_LIBRARY_PATH="${_ASCEND}/tools/tikicpulib/lib:${_ASCEND}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-60}"
fi

_patch_tiling_mix_pass() {
    python3 -c "
import numpy as np, sys
p='${COMPUTE_IO}/input/tiling.bin'
t=np.fromfile(p, dtype=np.int32, count=16)
t[2]=int(sys.argv[1])
t.tofile(p)
" "$1"
}

if [ "${VEC_SKIP_REBUILD:-0}" = "1" ] && [ -f "${OUT_INSTALL}/bin/ascendc_kernels_bbit" ]; then
    echo "[keygen-compute] VEC_SKIP_REBUILD=1 — reuse ${OUT_INSTALL}/bin/ascendc_kernels_bbit"
else
    rm -rf "${BUILD_DIR}" "${OUT_INSTALL}"
    mkdir -p "${BUILD_DIR}"
    cmake -B "${BUILD_DIR}" \
        -S "${CURRENT_DIR}/cmake/compute" \
        -DRUN_MODE="${RUN_MODE}" \
        -DSOC_VERSION="${SOC_VERSION}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${OUT_INSTALL}" \
        -DASCEND_CANN_PACKAGE_PATH="${_ASCEND}" \
        -DF203_STAGE1_SPLIT=1 \
        -DHAT_LINE18_DOT_ONLY=0 \
        -DHAT_BYTE_ENCODE=1 \
        -DF203_PIPELINE_PROBE=0 \
        -DF203_KEYGEN_EK_PKE=1 \
        -DHAT_ALG11_VEC=1 \
        -DBYTE_ENCODE12_VEC=1 \
        -DBYTE_ENCODE12_SCATTER_VEC=1 \
        -DBYTE_ENCODE12_PREFETCH=1 \
        -DALG11_IMPL=1 \
        -DALG11_VEC_VARIANT=2 \
        -DALG11_VEC_OPTS=1 \
        -DALG11_MEM_OPS=1
    cmake --build "${BUILD_DIR}" -j"$(nproc)"
    cmake --install "${BUILD_DIR}"
fi

mkdir -p "${COMPUTE_IO}/input" "${COMPUTE_IO}/output"
rm -rf "${COMPUTE_IO}/output"/*
export LD_LIBRARY_PATH="${OUT_INSTALL}/lib:${OUT_INSTALL}/lib64:${CURRENT_DIR}/out/lib:${CURRENT_DIR}/out/lib64:${_ASCEND}/lib64:${LD_LIBRARY_PATH:-}"
rm -f "${CURRENT_DIR}/ascendc_kernels_bbit"
cp -f "${OUT_INSTALL}/bin/ascendc_kernels_bbit" "${CURRENT_DIR}/"

cd "${COMPUTE_IO}"
_patch_tiling_mix_pass 0
bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "${CURRENT_DIR}/ascendc_kernels_bbit"
if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${COMPUTE_IO}"
fi
