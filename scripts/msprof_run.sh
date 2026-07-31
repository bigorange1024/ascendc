#!/usr/bin/env bash
# @file scripts/msprof_run.sh
# @brief kernel 启动包装：**默认不采集** msprof；显式 RUN_WITH_MSPROF=1 时在 sim / npu 下走 msprof op。
#
# 背景（2026-07-31，借入 910B4 实机）：各 ML-KEM 探针 run.sh 只经 kernel-run-timeout.sh 直跑二进制，
# 没有 msprof 通路，实机上 export RUN_WITH_MSPROF=1 会被静默忽略、不产任何 OPPROF_*。本脚本把
# add_custom/run.sh 里已验证的 msprof op 用法抽成共用函数，供各探针一行接入，同时保证：
#   * 缺省行为**逐字不变** —— 仍是 bash scripts/kernel-run-timeout.sh <bin>（含防挂死预算与 CAModel 日志绑定）；
#   * 采集产物只落 <用例>/prof_<mode>/<bin>/ 下（OPPROF_* 由 msprof 建在该 --output 目录内），
#     不污染用例根目录（见 Rule「Agent 跑用例验收」的 stray dump 检查）。
#
# 用法（在用例 run.sh 内，紧邻原 kernel 启动行）：
#   source "${REPO_ROOT}/scripts/msprof_run.sh"
#   msprof_run_kernel "${CURRENT_DIR}/ascendc_keygen_bbit"
#
# 采集（须显式指定，非默认）：
#   RUN_WITH_MSPROF=1 bash run.sh -r npu -v Ascend910B4    # 实机 msprof op
#   RUN_WITH_MSPROF=1 bash run.sh -r sim -v Ascend910B4    # CAModel msprof op simulator（很慢）
#
# 可调环境变量：
#   MSPROF_AIC_METRICS_NPU   实机指标集，默认 PipeUtilization,MemoryUB,Memory
#   MSPROF_AIC_METRICS_SIM   仿真指标集，默认 PipeUtilization（simulator 仅支持少数几个）
#   MSPROF_LAUNCH_COUNT_NPU  实机采集的 launch 数，默认 8（多 launch 的 device session 需调大）
#   MSPROF_LAUNCH_COUNT_SIM  仿真采集的 launch 数，默认 1
#   MSPROF_TIMEOUT_MIN       msprof 自身 --timeout，单位分钟，默认 60
#   MSPROF_WALL_TIMEOUT_SEC  外层墙钟超时秒数，默认 1800；0 = 不限制
#   MSPROF_OUTPUT_DIR        采集根目录，默认 $(pwd)/prof_<RUN_MODE>
#   MSPROF_SOC_VERSION       simulator 的 --soc-version，默认取 SIM_DEVICE / SOC_VERSION / Ascend910B4

_MSPROF_RUN_REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 直跑（默认路径）：完全交给 kernel-run-timeout.sh，保持预算与 CAModel 日志行为一致。
_msprof_run_direct() {
    bash "${_MSPROF_RUN_REPO_ROOT}/scripts/kernel-run-timeout.sh" "$@"
}

# 启动 kernel：按 RUN_WITH_MSPROF 与 RUN_MODE 决定直跑还是 msprof 采集。
# $1 = 可执行文件路径；其余参数原样透传给它。
msprof_run_kernel() {
    if [ "$#" -lt 1 ]; then
        echo "[msprof_run] usage: msprof_run_kernel <binary> [args...]" >&2
        return 1
    fi

    if [ "${RUN_WITH_MSPROF:-0}" != "1" ]; then
        _msprof_run_direct "$@"
        return
    fi

    local mode="${RUN_MODE:-cpu}"
    # CPU 孪生是 host 侧模拟，没有 AI Core 硬件计数器，msprof 无意义 → 退回直跑并说明。
    if [ "${mode}" != "sim" ] && [ "${mode}" != "npu" ]; then
        echo "[msprof_run] RUN_MODE=${mode} 不支持 msprof（仅 sim / npu），按默认路径直跑" >&2
        _msprof_run_direct "$@"
        return
    fi
    if ! command -v msprof >/dev/null 2>&1; then
        echo "[msprof_run] ERROR: 未找到 msprof；请先 source ${_MSPROF_RUN_REPO_ROOT}/scripts/env.sh" >&2
        return 1
    fi

    local bin="$1"
    shift
    local bin_name
    bin_name="$(basename "${bin}")"
    # 采集根目录按模式分开；每个二进制一个子目录，便于 Decaps 这类一次跑多个 phase 的探针各自留档。
    local prof_root="${MSPROF_OUTPUT_DIR:-$(pwd)/prof_${mode}}"
    local prof_out="${prof_root}/${bin_name}"
    rm -rf "${prof_out}"
    mkdir -p "${prof_out}"

    # 与 kernel-run-timeout.sh 一致：按当前工作目录重新绑定 CAModel / ADX 日志路径，
    # 避免 shell 残留旧用例路径，也避免 dump 落到用例根目录。
    # shellcheck source=/dev/null
    source "${_MSPROF_RUN_REPO_ROOT}/scripts/camodel_sim_log.sh" "$(pwd)"

    local -a cmd
    if [ "${mode}" = "sim" ]; then
        local soc="${MSPROF_SOC_VERSION:-${SIM_DEVICE:-${SOC_VERSION:-Ascend910B4}}}"
        echo "[msprof_run] msprof op simulator soc=${soc} → ${prof_out}"
        echo "[msprof_run] CAModel 采集通常远慢于 SIM_DIRECT 直跑，数分钟～数十分钟属正常"
        cmd=(
            msprof op simulator "${bin}"
            --soc-version="${soc}"
            --output="${prof_out}"
            --aic-metrics="${MSPROF_AIC_METRICS_SIM:-PipeUtilization}"
            --launch-count="${MSPROF_LAUNCH_COUNT_SIM:-1}"
            --timeout="${MSPROF_TIMEOUT_MIN:-60}"
        )
    else
        echo "[msprof_run] msprof op (device=${ASCEND_DEVICE_ID:-n/a}) → ${prof_out}"
        cmd=(
            msprof op "${bin}"
            --output="${prof_out}"
            --aic-metrics="${MSPROF_AIC_METRICS_NPU:-PipeUtilization,MemoryUB,Memory}"
            --launch-count="${MSPROF_LAUNCH_COUNT_NPU:-8}"
        )
    fi
    cmd+=("$@")

    local rc=0
    local wall="${MSPROF_WALL_TIMEOUT_SEC:-1800}"
    if [ "${wall}" != "0" ] && command -v timeout >/dev/null 2>&1; then
        timeout --foreground "${wall}" "${cmd[@]}" || rc=$?
    else
        "${cmd[@]}" || rc=$?
    fi
    if [ "${rc}" -ne 0 ]; then
        echo "[msprof_run] ERROR: msprof 退出码 ${rc}（124=墙钟超时 MSPROF_WALL_TIMEOUT_SEC=${wall}s）" >&2
        return "${rc}"
    fi

    # 采集成功但目录里没有 csv，多半是 launch-count / 指标集不被该版本支持 —— 提示而不假绿。
    local csv_count
    csv_count="$(find "${prof_out}" -name '*.csv' 2>/dev/null | wc -l)"
    if [ "${csv_count}" -eq 0 ]; then
        echo "[msprof_run] WARN: ${prof_out} 下未见 csv，性能数据可能未落盘" >&2
    else
        echo "[msprof_run] OK: ${csv_count} 个 csv 于 ${prof_out}"
        find "${prof_out}" -maxdepth 1 -name 'OPPROF_*' 2>/dev/null || true
    fi
}
