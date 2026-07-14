#!/bin/bash
# pass-fix-f203-alg7-sample-ntt-k4 — FIPS 203 Alg.7 SampleNTT（单 poly 模块）
#
# 本脚本为探针 pass-fix-f203-alg7-sample-ntt-k4 的端到端验收入口：
#   1. 解析 CLI / 环境变量（种子、poly 坐标、rej 实现路径等）
#   2. CMake 编译 AscendC kernel（CPU 孪生或 CaModel SIM）
#   3. 生成 ROM 头文件、golden 输入/期望输出、运行 Python 语义自检
#   4. 启动 kernel（带超时）并调用 verify_result.py 对拍
#
# 数据流概要（与 FIPS 203 Alg.7 SampleNTT 对齐，几何见 f203_alg7_layout.h）：
#   SEED_D → derand d → G(d)→ρ → seed=ρ||j||i → SHAKE128 squeeze 672B xof
#   → 三字节解交织得 d1[224]/d2[224] → rej 采样得 â[256]（模 q=3329）
#
# Usage（默认）：
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# 环境变量：
#   SEED_D         — derand 种子（默认 20260619）
#   ALG7_POLY_J    — poly 列 j（默认 0）
#   ALG7_POLY_I    — poly 行 i（默认 0）
#   F203_ALG7_REJ_IMPL — 0=标量对照 | 1=vec_mins（**默认**）| 2=vec_mask
#   SE_ALG7_REJ        — 同上别名：scalar|0、vec|vec_mins|1、vec_mask|2（优先于 IMPL 数值）
#   F203_ALG7_DUMP_XOF — 0（默认，跳过 xof dump）| 1（调试对拍 xof golden）
#   F203_ALG7_D12_GATHER — 0（默认，标量解交织）| 1（实验 Gather+ROM，SIM 负优化）
#
# 解析本脚本所在目录与仓库根目录（后续 source env、camodel 日志均依赖 REPO_ROOT）
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
REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"

# CMake / 运行默认参数（可通过 -r/-v/-b/-p 覆盖）
BUILD_TYPE="Debug"
INSTALL_PREFIX="${CURRENT_DIR}/out"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"

# getopt 长/短选项：run-mode、soc-version、CANN 安装路径、build-type、install-prefix
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
# 探测 CANN / Ascend Toolkit 安装路径（优先级：显式 -i > ASCEND_HOME > 常见默认路径）
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

# Alg.7 golden / kernel 共用的运行时参数（gen_data.py 与 main.cpp 均读取同名 env）
export SEED_D="${SEED_D:-20260619}"
export ALG7_POLY_J="${ALG7_POLY_J:-0}"
export ALG7_POLY_I="${ALG7_POLY_I:-0}"

# rej 实现路径：SE_ALG7_REJ 字符串别名优先于 F203_ALG7_REJ_IMPL 数值
if [ -n "${SE_ALG7_REJ:-}" ]; then
    case "${SE_ALG7_REJ}" in
    scalar | 0) F203_ALG7_REJ_IMPL=0 ;;
    vec | vec_mins | 1) F203_ALG7_REJ_IMPL=1 ;;
    vec_mask | 2) F203_ALG7_REJ_IMPL=2 ;;
    *) echo "[ERROR] SE_ALG7_REJ must be scalar|0|vec|vec_mins|1|vec_mask|2, got ${SE_ALG7_REJ}"; exit 1 ;;
    esac
else
    F203_ALG7_REJ_IMPL="${F203_ALG7_REJ_IMPL:-1}"
fi
export F203_ALG7_REJ_IMPL
export F203_ALG7_DUMP_XOF="${F203_ALG7_DUMP_XOF:-0}"
export F203_ALG7_D12_GATHER="${F203_ALG7_D12_GATHER:-0}"
echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} SEED_D=${SEED_D} j=${ALG7_POLY_J} i=${ALG7_POLY_I} F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL} F203_ALG7_DUMP_XOF=${F203_ALG7_DUMP_XOF} F203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}"

# 加载 CANN 环境：优先本仓 env.sh，否则 toolkit setenv.bash
if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -f "${_ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
fi

# 按运行模式设置动态库路径：SIM 仅需 simulator；CPU 孪生需 tikicpulib + simulator
if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
fi

set -e
# 全量重建：避免旧 CMake cache 与 ROM/golden 不同步
rm -rf build out
mkdir -p build
cmake -B build \
    -DRUN_MODE=${RUN_MODE} \
    -DSOC_VERSION=${SOC_VERSION} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
    -DASCEND_CANN_PACKAGE_PATH=${_ASCEND_INSTALL_PATH} \
    -DF203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL} \
    -DF203_ALG7_DUMP_XOF=${F203_ALG7_DUMP_XOF} \
    -DF203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER}
cmake --build build -j
cmake --install build

# 将 kernel 可执行文件拷到用例根目录，清空并重建 input/output
rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output

# --- Golden 与设备侧 ROM 生成链（顺序固定）---
# 1–3. 生成 Gather 索引 ROM 头文件（d12 交错、xof 解交织、rej compact LUT）
python3 "${CURRENT_DIR}/scripts/gen_alg7_interleave_rom.py"
python3 "${CURRENT_DIR}/scripts/gen_alg7_deinterleave_rom.py"
python3 "${CURRENT_DIR}/scripts/gen_alg7_compact_lut.py"
# 4. 主 golden：xof[672]、d1/d2[224]、â[256] 及 host 输入 seed_d / poly_ij
python3 "${CURRENT_DIR}/scripts/gen_data.py"
# 5–7. 语义自检：C 标量 rej、剔除双方案等价、多种子 spec==bulk
python3 "${CURRENT_DIR}/scripts/test_rej_scalar_c.py"
python3 "${CURRENT_DIR}/scripts/test_rej_filter_semantics.py"
python3 "${CURRENT_DIR}/scripts/test_multi_seed.py"

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

# kernel 计算段：本探针 rej+XOF 略重，默认预算 60s（仍经 kernel-run-timeout.sh 强制退出）
export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-60}"
if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi
bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
if [ "${RUN_MODE}" = "sim" ]; then
    # 收拢 stray dump 到 OPPROF_*/dump/，避免用例根目录残留 profile 产物
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

# 对拍 kernel 输出的 d1/d2/â（及可选 xof）与 golden
python3 "${CURRENT_DIR}/scripts/verify_result.py"
echo "[SUCCESS] pass-fix-f203-alg7-sample-ntt-k4 (${RUN_MODE})"
