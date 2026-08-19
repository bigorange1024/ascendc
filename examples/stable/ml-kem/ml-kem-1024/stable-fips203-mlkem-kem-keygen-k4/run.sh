#!/usr/bin/env bash
# stable-fips203-mlkem-kem-keygen-k4 — Alg.19 ML-KEM.KeyGen（2 launch · vendored PKE + 内嵌 Alg.16 尾）
#
# customspec：本目录 stable-fips203-mlkem-kem-keygen-k4-实现方案-customspec.{tex,pdf}
# 踩坑：Encode 后双 AIV SyncAll；CPU 由 AIV1 做 Fuse/Tail；KYBER_PIPE_ALL 恒真实；清零 output 多轮压测
#
# Usage（默认 = 生产全量 + golden 对拍；未写 -r 仍为 cpu）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4          # sim 内默认 SIM_DIRECT=1（CAModel）；勿手写
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#
# 调试（非默认）：
#   SEED_D=20260619 …        # 定点复现旧 KAT；默认不写死，由 SHA3 派生 host seed_d
#   SIM_DIRECT=0 … -r sim …  # msprof + OPPROF_*（慢；非默认）
#   KEM_KG_EXT_SEED=1 …          # 旁路 A：input/kem_seed.bin = d‖z
#   KEM_KEYGEN_FORCE_REBUILD=1 … # 强制重编
#   KEM_KEYGEN_VERIFY=0 …        # 跳过 liboqs golden（仅尺寸检查）
#   bash run.sh -r npu …         # 仅真机；WSL 由 runtime_env 拒绝
#   RUN_WITH_MSPROF=1 MSPROF_MODE=app … -r npu  # 整进程 profiling + [npu_launch]
#
# 分流：scripts/runtime_env.sh · docs/engineering/NPU真机环境说明.md

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
_ORIG_ARGS=("$@")

if [ "${KEM_KEYGEN_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_liboqs_kem_keygen.log" 2>&1
fi

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
SCRIPTS_PREP="${CURRENT_DIR}/scripts/prep"
INSTALL_PREFIX=""

# 默认不写死 SEED_D；未 export 时 prepare/gen_data 用 SHA3 派生（scripts/resolve_host_seed_d.py）
# 定点：SEED_D=20260619 bash run.sh …
if [ -n "${SEED_D:-}" ]; then
    export SEED_D
fi
export KEM_KEYGEN_VERIFY="${KEM_KEYGEN_VERIFY:-1}"
export KEM_KEYGEN_SKIP_REBUILD="${KEM_KEYGEN_SKIP_REBUILD:-${KEM_SKIP_REBUILD:-0}}"
export KEM_KEYGEN_FORCE_REBUILD="${KEM_KEYGEN_FORCE_REBUILD:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEM_KEYGEN_KERNEL_BUDGET_SEC:-900}"
export KEM_KG_EXT_SEED="${KEM_KG_EXT_SEED:-0}"

BUILD_TYPE="Debug"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"

SHORT=r:,v:,i:,b:,p:
LONG=run-mode:,soc-version:,install-path:,build-type:,install-prefix:
OPTS=$(getopt -a --options "$SHORT" --longoptions "$LONG" -- "$@")
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

if [ "${KEM_KG_EXT_SEED}" = "1" ]; then
    _DEFAULT_PROFILE="extseed"
else
    _DEFAULT_PROFILE="prod"
fi
BUILD_PROFILE="${KEM_KEYGEN_BUILD_PROFILE:-${_DEFAULT_PROFILE}}"
BUILD_DIR="${CURRENT_DIR}/build_${BUILD_PROFILE}_${RUN_MODE}"
if [ -z "${INSTALL_PREFIX}" ]; then
    INSTALL_PREFIX="${CURRENT_DIR}/out_${BUILD_PROFILE}_${RUN_MODE}"
fi

# CANN：统一 ${REPO_ROOT}/scripts/env.sh（多候选 set_env + 回写 CANN_HOME）；禁止写死 ~/ascendc，
# 否则仓库不在 $HOME/ascendc 的机器（如借入实机）会静默落到错误的 toolkit。
# 保存/恢复 errexit：本段靠返回码判断 source 结果，不改变脚本原有的 set -e 状态。
_had_errexit=0
case $- in *e*) _had_errexit=1 ;; esac
set +e
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/env.sh"
_env_rc=$?
[ "${_had_errexit}" = "1" ] && set -e
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

# 实机 ACL 设备号：npu 按树分卡（stable=1 / examples=2 / tests=3，见 npu_device_map.sh）；显式 ASCEND_DEVICE_ID 优先。SIM 强制 0
if [ "${RUN_MODE}" = "npu" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/npu_device_map.sh"
    npu_device_map_apply "${CURRENT_DIR}"
elif [ "${RUN_MODE}" = "sim" ]; then
    export ASCEND_DEVICE_ID=0
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT="${SIM_DIRECT:-1}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "npu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
fi

_keygen_gen_alg7_roms() {
    mkdir -p "${CURRENT_DIR}/prep/alg7"
    export PYTHONPATH="${CURRENT_DIR}/scripts/prep:${PYTHONPATH:-}"
    python3 "${SCRIPTS_PREP}/gen_alg7_interleave_rom.py"
    python3 "${SCRIPTS_PREP}/gen_alg7_deinterleave_rom.py"
    python3 "${SCRIPTS_PREP}/gen_alg7_compact_lut.py"
}

_build_stamp="${BUILD_DIR}/.kem_keygen_run_mode"
_build_tag="${BUILD_PROFILE}:${RUN_MODE}:extseed=${KEM_KG_EXT_SEED}"
_need_build=1
if [ "${KEM_KEYGEN_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${KEM_KEYGEN_SKIP_REBUILD}" = "1" ] && [ -x "${INSTALL_PREFIX}/bin/ascendc_kem_keygen_bbit" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_kem_build() {
    if [ "${KEM_KEYGEN_FORCE_REBUILD}" = "1" ] || \
       { [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" != "${_build_tag}" ]; }; then
        rm -rf "${BUILD_DIR}" "${INSTALL_PREFIX}"
    fi
    mkdir -p "${BUILD_DIR}"
    _keygen_gen_alg7_roms
    cmake -B "${BUILD_DIR}" \
        -S "${CURRENT_DIR}/cmake/keygen" \
        -DRUN_MODE="${RUN_MODE}" \
        -DSOC_VERSION="${SOC_VERSION}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}" \
        -DF203_AHAT16_BLOCK_DIM=2 \
        -DF203_ALG7_REJ_IMPL=1 \
        -DF203_ALG7_D12_GATHER=0 \
        -DF203_AHAT16_BATCH_SHAKE=0 \
        -DF203_ALG7_XOF_504=0 \
        -DF203_CBD_BLOCK_DIM=1 \
        -DF203_STAGE1_SPLIT=1 \
        -DHAT_LINE18_DOT_ONLY=0 \
        -DHAT_BYTE_ENCODE=1 \
        -DF203_PIPELINE_PROBE=0 \
        -DHAT_ALG11_VEC=1 \
        -DBYTE_ENCODE12_VEC=1 \
        -DBYTE_ENCODE12_SCATTER_VEC=1 \
        -DBYTE_ENCODE12_PREFETCH=1 \
        -DALG11_IMPL=1 \
        -DALG11_VEC_VARIANT=2 \
        -DALG11_VEC_OPTS=1 \
        -DALG11_MEM_OPS=1 \
        -DF203_KEM_KEYGEN_TAIL=1 \
        -DKEM_KG_EXT_SEED="${KEM_KG_EXT_SEED}"
    cmake --build "${BUILD_DIR}" -j"${CMAKE_BUILD_JOBS}"
    cmake --install "${BUILD_DIR}"
    echo "${_build_tag}" >"${_build_stamp}"
}

set -e
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"
# 防残留掩盖抢跑：默认 VERIFY 路径每次清零 output（KAT 旁路保留自写 bin）
if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
    rm -f "${CURRENT_DIR}/output/"*.bin
fi
python3 "${CURRENT_DIR}/scripts/prepare_production_input.py"
# 与 input/seed_d.bin 对齐，供后续 log / SUCCESS（默认 SHA3 派生或 env 覆盖）
export SEED_D="$(python3 "${CURRENT_DIR}/scripts/resolve_host_seed_d.py")"
if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
    echo "[kem_keygen] RUN_MODE=${RUN_MODE} SEED_D=${SEED_D} profile=${BUILD_PROFILE} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC}"
fi
if [ "${_need_build}" = "1" ]; then
    if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
        echo "[run.sh] build profile=${BUILD_PROFILE} RUN_MODE=${RUN_MODE} dir=${BUILD_DIR##*/} jobs=${CMAKE_BUILD_JOBS}"
    fi
    _kem_build
else
    if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
        echo "[run.sh] skip rebuild (profile=${BUILD_PROFILE}, RUN_MODE=${RUN_MODE})"
    fi
fi

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${INSTALL_PREFIX}/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
rm -f "${CURRENT_DIR}/ascendc_kem_keygen_bbit"
cp -f "${INSTALL_PREFIX}/bin/ascendc_kem_keygen_bbit" "${CURRENT_DIR}/"

if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi

# 默认直跑（等价于原 kernel-run-timeout.sh）；RUN_WITH_MSPROF=1 时在 sim/npu 下走 msprof op。
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/msprof_run.sh"
msprof_run_kernel "${CURRENT_DIR}/ascendc_kem_keygen_bbit"

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

if [ "${KEM_KEYGEN_KAT:-0}" = "1" ]; then
    ek_sz=$(wc -c <"${CURRENT_DIR}/output/ek_kem.bin")
    dk_sz=$(wc -c <"${CURRENT_DIR}/output/dk_kem.bin")
    if [ "${ek_sz}" -ne 1568 ] || [ "${dk_sz}" -ne 3168 ]; then
        echo "[ERROR] output size ek=${ek_sz} dk=${dk_sz}"
        exit 1
    fi
elif [ "${KEM_KEYGEN_VERIFY:-0}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/gen_data.py"
    python3 "${CURRENT_DIR}/scripts/verify_kem.py"
else
    ek_sz=$(wc -c <"${CURRENT_DIR}/output/ek_kem.bin")
    dk_sz=$(wc -c <"${CURRENT_DIR}/output/dk_kem.bin")
    if [ "${ek_sz}" -ne 1568 ] || [ "${dk_sz}" -ne 3168 ]; then
        echo "[ERROR] output size ek=${ek_sz} dk=${dk_sz}"
        exit 1
    fi
    if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
        echo "[kem_keygen incubating] output OK ek_kem=${ek_sz}B dk_kem=${dk_sz}B"
    fi
fi

if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
    echo "[SUCCESS] stable-fips203-mlkem-kem-keygen-k4 (${RUN_MODE}) SEED_D=${SEED_D}"
fi
