#!/bin/bash
# =============================================================================
# pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2 — 活跃 MLKEM 行 16–20 向量集成基线
# =============================================================================
#
# 流水线：host src[8,256] → S1 limb → S2 MMAD → S3 merge → 行18 j→p 内积 → 行19–20 ByteEncode
#
# Usage（默认 = Alg.13 行 16–20 全链路，含 ê 与 ByteEncode₁₂）：
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 调试分段（须显式指定，非默认）：
#   NTTS2S1E_MIX_PASS=6 MMAD_SANITY_CASE=eye16 bash run.sh -r sim   # AicMmad 隔离 sanity（tag5t 同维 16×256×128）
#   HAT_LINE18_DOT_ONLY=1 HAT_BYTE_ENCODE=0 bash run.sh -r sim -v Ascend910B4  # 仅 dot-only ~65k tick
#
# 环境变量（CMake 编译期 + 运行时）：
#   NTTS2S1E_MIX_PASS / TAG5T_MIX_PASS — 0=全链路（默认）
#   HAT_LINE18_DOT_ONLY — 默认 0（+ê+mod）；1=仅 Â·ŝ（调试）
#   HAT_BYTE_ENCODE     — 默认 1（行19–20）；0=跳过编码（调试）
#   HAT_LINE18_FULLPOLY — 1=单 TPipe j→p（生产）
#   F203_STAGE1_SPLIT   — 0 标量 / 1 bulk向量（默认）/ 2 tile32
#   F203_MOD_VARIANT    — 设备 mod 变体（golden 仍 C 标量）
#   F203_PIPELINE_PROBE — 1=CPU 开发期 UB 抽样（pipeline_probe.hpp）
#   F203_KEYGEN_EK_PKE   — 默认 1：pass4 末次 launch 内核融合 ek_polyvec‖ρ→ek_PKE（行 21，无额外 launch）
#
# mixPass=0：单次 launch，S1→S2 MMAD→S3→行18–21 全在 AscendC（含 F203_KEYGEN_EK_PKE 行 21 融合）。
# KEYGEN_ORCHESTRATE=1（G4 编排）：上游写入 a_hat/src/rho；compute 单 launch mixPass=0，禁止 Host NTT/fixup。
# 详见：IMPLEMENTATION_REFERENCE.md、STATUS.md、pass-fix-f203-alg13-device-keygen-k4/INTEGRATION_PLAN.md
# =============================================================================
#
# 多环境分流（scripts/runtime_env.sh）：
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#   WSL 禁止 -r npu；说明见 docs/engineering/NPU真机环境说明.md
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
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
if [ -n "$ASCEND_INSTALL_PATH" ]; then
    _ASCEND_INSTALL_PATH=$ASCEND_INSTALL_PATH
elif [ -n "$ASCEND_HOME_PATH" ]; then
    _ASCEND_INSTALL_PATH=$ASCEND_HOME_PATH
elif [ -d "$HOME/Ascend/ascend-toolkit/latest" ]; then
    _ASCEND_INSTALL_PATH=$HOME/Ascend/ascend-toolkit/latest
elif [ -d "$HOME/Ascend/cann" ]; then
    _ASCEND_INSTALL_PATH=$HOME/Ascend/cann
else
    _ASCEND_INSTALL_PATH=/usr/local/Ascend/ascend-toolkit/latest
fi

export ASCEND_TOOLKIT_HOME=${_ASCEND_INSTALL_PATH}
export ASCEND_HOME_PATH=${_ASCEND_INSTALL_PATH}
export NTTS2S1E_MIX_PASS="${NTTS2S1E_MIX_PASS:-0}"
export F203_STAGE1_SPLIT="${F203_STAGE1_SPLIT:-1}"
export HAT_LINE18_DOT_ONLY="${HAT_LINE18_DOT_ONLY:-0}"
export HAT_BYTE_ENCODE="${HAT_BYTE_ENCODE:-1}"
export F203_PIPELINE_PROBE="${F203_PIPELINE_PROBE:-0}"
export F203_PROBE_EARLY="${F203_PROBE_EARLY:-0}"
export HAT_ALG11_VEC="${HAT_ALG11_VEC:-1}"
export BYTE_ENCODE12_VEC="${BYTE_ENCODE12_VEC:-1}"
export BYTE_ENCODE12_SCATTER_VEC="${BYTE_ENCODE12_SCATTER_VEC:-1}"
export BYTE_ENCODE12_PREFETCH="${BYTE_ENCODE12_PREFETCH:-1}"
export ALG11_IMPL="${ALG11_IMPL:-1}"
export ALG11_VEC_VARIANT="${ALG11_VEC_VARIANT:-2}"
export ALG11_VEC_OPTS="${ALG11_VEC_OPTS:-1}"
export ALG11_MEM_OPS="${ALG11_MEM_OPS:-1}"
echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} NTTS2S1E_MIX_PASS=${NTTS2S1E_MIX_PASS} HAT_LINE18_DOT_ONLY=${HAT_LINE18_DOT_ONLY} HAT_BYTE_ENCODE=${HAT_BYTE_ENCODE} F203_KEYGEN_EK_PKE=${F203_KEYGEN_EK_PKE} F203_PIPELINE_PROBE=${F203_PIPELINE_PROBE} F203_PROBE_EARLY=${F203_PROBE_EARLY} F203_STAGE1_SPLIT=${F203_STAGE1_SPLIT} HAT_ALG11_VEC=${HAT_ALG11_VEC} BYTE_ENCODE12_VEC=${BYTE_ENCODE12_VEC} BYTE_ENCODE12_PREFETCH=${BYTE_ENCODE12_PREFETCH} ALG11_IMPL=${ALG11_IMPL} ALG11_VEC_VARIANT=${ALG11_VEC_VARIANT} ALG11_MEM_OPS=${ALG11_MEM_OPS}"

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -f "${_ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
fi

set -e
# KeyGen 等上游编排已写入 input/；禁止 gen_data 覆盖 a_hat/src（见 pass-fix-f203-alg13-device-keygen-k4）
ORCHESTRATE="${KEYGEN_ORCHESTRATE:-0}"
F203_KEYGEN_EK_PKE="${F203_KEYGEN_EK_PKE:-1}"
export F203_KEYGEN_EK_PKE

_patch_tiling_mix_pass() {
    python3 -c "
import numpy as np, sys
p='input/tiling.bin'
t=np.fromfile(p, dtype=np.int32, count=16)
t[2]=int(sys.argv[1])
t.tofile(p)
" "$1"
}

if [ "${VEC_SKIP_REBUILD:-0}" = "1" ] && [ -f "./out/bin/ascendc_kernels_bbit" ]; then
    echo "[vec-k4-v2] VEC_SKIP_REBUILD=1 — reuse ./out/bin/ascendc_kernels_bbit"
else
rm -rf build out
mkdir -p build
cmake -B build \
    -DRUN_MODE=${RUN_MODE} \
    -DSOC_VERSION=${SOC_VERSION} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
    -DASCEND_CANN_PACKAGE_PATH=${_ASCEND_INSTALL_PATH} \
    -DF203_STAGE1_SPLIT=${F203_STAGE1_SPLIT} \
    -DHAT_LINE18_DOT_ONLY=${HAT_LINE18_DOT_ONLY} \
    -DHAT_BYTE_ENCODE=${HAT_BYTE_ENCODE} \
    -DF203_PIPELINE_PROBE=${F203_PIPELINE_PROBE} \
    -DF203_KEYGEN_EK_PKE=${F203_KEYGEN_EK_PKE} \
    -DHAT_ALG11_VEC=${HAT_ALG11_VEC} \
    -DBYTE_ENCODE12_VEC=${BYTE_ENCODE12_VEC} \
    -DBYTE_ENCODE12_SCATTER_VEC=${BYTE_ENCODE12_SCATTER_VEC} \
    -DBYTE_ENCODE12_PREFETCH=${BYTE_ENCODE12_PREFETCH} \
    -DALG11_IMPL=${ALG11_IMPL} \
    -DALG11_VEC_VARIANT=${ALG11_VEC_VARIANT} \
    -DALG11_VEC_OPTS=${ALG11_VEC_OPTS} \
    -DALG11_MEM_OPS=${ALG11_MEM_OPS}
cmake --build build -j
cmake --install build
fi

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
if [ "${ORCHESTRATE}" = "1" ]; then
    mkdir -p input output
    rm -rf output/*
    echo "[vec-k4-v2] KEYGEN_ORCHESTRATE=1 — keep upstream input/, skip gen_data.py"
elif [ "${NTTS2S1E_MIX_PASS}" = "6" ] || [ "${F203_MMAD_SANITY:-0}" = "1" ]; then
    rm -rf input output
    mkdir -p input output
    export NTTS2S1E_MIX_PASS=6
    python3 "${CURRENT_DIR}/scripts/gen_mmad_sanity_data.py"
    if [ "${F203_KEYGEN_EK_PKE}" = "1" ]; then
        python3 -c "import numpy as np; np.zeros(32,dtype=np.uint8).tofile('input/rho.bin')"
    fi
else
    rm -rf input output
    mkdir -p input output
    python3 "${CURRENT_DIR}/scripts/gen_data.py"
fi

if [ "${NTTS2S1E_MIX_PASS}" = "2" ]; then
    if [ -f "./output/s0.bin" ]; then
        cp ./output/s0.bin ./input/s0_preset.bin
    elif [ -f "./output/golden_s0.bin" ]; then
        cp ./output/golden_s0.bin ./input/s0_preset.bin
    fi
fi
if [ "${NTTS2S1E_MIX_PASS}" = "3" ]; then
    if [ -f "./output/mat_c.bin" ]; then
        cp ./output/mat_c.bin ./input/mat_c_preset.bin
    elif [ -f "./output/golden_mat_c.bin" ]; then
        cp ./output/golden_mat_c.bin ./input/mat_c_preset.bin
    fi
fi
if [ "${NTTS2S1E_MIX_PASS}" = "4" ] || [ "${NTTS2S1E_MIX_PASS}" = "7" ]; then
    if [ -f "./output/dst.bin" ]; then
        cp ./output/dst.bin ./input/dst_preset.bin
    elif [ -f "./output/golden.bin" ]; then
        cp ./output/golden.bin ./input/dst_preset.bin
    fi
fi
if [ "${NTTS2S1E_MIX_PASS}" = "7" ]; then
    if [ -f "./output/golden_t_hat.bin" ]; then
        cp ./output/golden_t_hat.bin ./input/t_hat_preset.bin
    elif [ -f "./output/t_hat.bin" ]; then
        cp ./output/t_hat.bin ./input/t_hat_preset.bin
    fi
fi

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT=1
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-600}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
else
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-60}"
fi
if [ "${ORCHESTRATE}" = "1" ]; then
    # G4：单 launch mixPass=0 — 行 16–21 全 AscendC + ek‖ρ 内核融合
    export NTTS2S1E_MIX_PASS=0
    _patch_tiling_mix_pass 0
    bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
    if [ "${F203_PROBE_EARLY}" = "1" ]; then
        python3 "${CURRENT_DIR}/scripts/probe_stage_verify.py" || true
    fi
else
    bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
    if [ "${F203_PROBE_EARLY}" = "1" ]; then
        python3 "${CURRENT_DIR}/scripts/probe_stage_verify.py" || true
    fi
fi
if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

if [ "${ORCHESTRATE}" = "1" ]; then
    echo "[vec-k4-v2] KEYGEN_ORCHESTRATE=1 — skip local verify (upstream KeyGen G4)"
elif [ "${NTTS2S1E_MIX_PASS}" = "6" ] || [ "${F203_MMAD_SANITY:-0}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_mmad_sanity.py"
    echo "[SUCCESS] pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2 MMAD sanity (${RUN_MODE})"
else
    python3 "${CURRENT_DIR}/scripts/verify_result.py"
    echo "[SUCCESS] pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2 (${RUN_MODE})"
fi
