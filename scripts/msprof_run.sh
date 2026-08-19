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
# 采集（须显式指定，非默认；教材套件 npu 默认打开）：
#   RUN_WITH_MSPROF=1 MSPROF_MODE=app bash run.sh -r npu -v Ascend910B4
#   RUN_WITH_MSPROF=1 bash run.sh -r sim -v Ascend910B4    # CAModel msprof op simulator（很慢）
#
# MSPROF_MODE（npu）：
#   app — 默认（教材/KEM）：`msprof --application` 对**真实进程跑一遍**，csv 里每个 KernelLaunch 一行
#   op  — 旧路径 `msprof op --launch-count`：把二进制当「单个 op 重放」；KEM 多 launch **不要**用这个当整算子时间
#
# 可调环境变量：
#   MSPROF_MODE              npu 采集模式，默认 app
#   MSPROF_AIC_METRICS_NPU   实机指标集，默认 PipeUtilization,MemoryUB,Memory
#   MSPROF_AIC_METRICS_SIM   仿真指标集，默认 PipeUtilization（simulator 仅支持少数几个）
#   MSPROF_LAUNCH_COUNT_NPU  仅 MODE=op：实机 launch-count，默认 64（盖住 correctness 多段）
#   MSPROF_LAUNCH_COUNT_SIM  仿真采集的 launch 数，默认 1
#   MSPROF_TIMEOUT_MIN       msprof 自身 --timeout，单位分钟，默认 60
#   MSPROF_WALL_TIMEOUT_SEC  外层墙钟超时秒数，默认 1800；0 = 不限制
#   MSPROF_OUTPUT_DIR        采集根目录，默认 $(pwd)/prof_<RUN_MODE>
#   MSPROF_SOC_VERSION       simulator 的 --soc-version，默认取 SIM_DEVICE / SOC_VERSION / Ascend910B4
#   MSPROF_RUN_METRICS       默认 1：直跑 tee 到 output/run_metrics.txt 并打印 [run_metrics] 摘要
#   MSPROF_RUN_METRICS_FILE  默认 $(pwd)/output/run_metrics.txt
# 墙钟来源：scripts/kernel-run-timeout.sh 结束行 `[wall_sec]`（不依赖 /usr/bin/time）。
# 多 launch 真值：采集成功后调用 npu_msprof_summarize.py（kernel_details 求和 + host JSONL）。

_MSPROF_RUN_REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 从 run_metrics.txt 提取墙钟 / SIM Total tick，打印一行摘要（实机 npu 通常只有 wall_sec）。
_msprof_emit_run_metrics_summary() {
    local metrics_file="$1"
    if [ ! -f "${metrics_file}" ]; then
        return 0
    fi
    local mode="${RUN_MODE:-cpu}"
    local wall tick_sum tick_lines
    wall="$(grep -E '^\[wall_sec\]' "${metrics_file}" 2>/dev/null | tail -1 | awk '{print $2}')"
    tick_lines="$(grep -E 'Total tick:' "${metrics_file}" 2>/dev/null | wc -l | tr -d ' ')"
    if [ "${tick_lines:-0}" -gt 0 ]; then
        tick_sum="$(grep -E 'Total tick:' "${metrics_file}" | awk -F: '{gsub(/ /,"",$2); s+=$2} END {print s+0}')"
    else
        tick_sum=""
    fi
    if [ -n "${wall}" ]; then
        echo "[run_metrics] RUN_MODE=${mode} wall_sec=${wall} log=${metrics_file}"
    else
        echo "[run_metrics] RUN_MODE=${mode} log=${metrics_file}（未解析到 [wall_sec]）"
    fi
    if [ "${mode}" = "sim" ] && [ -n "${tick_sum}" ] && [ "${tick_sum}" != "0" ]; then
        echo "[run_metrics] sim_total_tick=${tick_sum} (${tick_lines} launch)"
    elif [ "${mode}" = "npu" ]; then
        echo "[run_metrics] 实机无 CAModel Total tick；逐 launch 见 [npu_launch]；csv 见 RUN_WITH_MSPROF=1"
        local jsonl="${NPU_LAUNCH_METRICS_FILE:-$(pwd)/output/npu_launch_metrics.jsonl}"
        if [ -f "${jsonl}" ]; then
            python3 "${_MSPROF_RUN_REPO_ROOT}/scripts/npu_msprof_summarize.py" --jsonl "${jsonl}" --metrics "${metrics_file}" "$(pwd)" 2>/dev/null || true
        fi
    fi
}

# 直跑（默认路径）：kernel-run-timeout.sh（自带 [wall_sec]）+ 可选 tee 台账。
_msprof_run_direct() {
    mkdir -p "$(pwd)/output"
    export NPU_LAUNCH_METRICS_FILE="${NPU_LAUNCH_METRICS_FILE:-$(pwd)/output/npu_launch_metrics.jsonl}"
    : >"${NPU_LAUNCH_METRICS_FILE}" 2>/dev/null || true
    if [ "${MSPROF_RUN_METRICS:-1}" = "1" ]; then
        local metrics_file="${MSPROF_RUN_METRICS_FILE:-$(pwd)/output/run_metrics.txt}"
        mkdir -p "$(dirname "${metrics_file}")"
        set -o pipefail
        bash "${_MSPROF_RUN_REPO_ROOT}/scripts/kernel-run-timeout.sh" "$@" 2>&1 | tee "${metrics_file}"
        local rc=${PIPESTATUS[0]}
        set +o pipefail
        _msprof_emit_run_metrics_summary "${metrics_file}"
        return "${rc}"
    fi
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

    mkdir -p "$(pwd)/output"
    : >"$(pwd)/output/npu_launch_metrics.jsonl" 2>/dev/null || true
    export NPU_LAUNCH_METRICS_FILE="${NPU_LAUNCH_METRICS_FILE:-$(pwd)/output/npu_launch_metrics.jsonl}"

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
        cmd+=("$@")
    else
        local npu_mode="${MSPROF_MODE:-app}"
        local metrics="${MSPROF_AIC_METRICS_NPU:-PipeUtilization,MemoryUB,Memory}"
        local app_cmd="${bin}"
        if [ "$#" -gt 0 ]; then
            printf -v app_cmd '%q ' "${bin}" "$@"
            app_cmd="${app_cmd% }"
        fi
        if [ "${npu_mode}" = "op" ]; then
            echo "[msprof_run] MODE=op msprof op launch-count=${MSPROF_LAUNCH_COUNT_NPU:-64} (device=${ASCEND_DEVICE_ID:-n/a}) → ${prof_out}"
            echo "[msprof_run] WARN: op 模式是单 op 重放；KEM 多 launch 请改 MSPROF_MODE=app，并以 kernel_details 求和为准"
            cmd=(
                msprof op "${bin}"
                --output="${prof_out}"
                --aic-metrics="${metrics}"
                --launch-count="${MSPROF_LAUNCH_COUNT_NPU:-64}"
            )
            cmd+=("$@")
        else
            echo "[msprof_run] MODE=app msprof --application (device=${ASCEND_DEVICE_ID:-n/a}) → ${prof_out}"
            echo "[msprof_run] 整进程一次采集；逐 kernel 以 kernel_details.csv 为准，勿信终端单行 task duration"
            cmd=(
                msprof
                --application="${app_cmd}"
                --output="${prof_out}"
            )
            # 部分 CANN 接受 --aic-metrics；不支持时下面失败再回退 op
            if [ -n "${metrics}" ]; then
                cmd+=(--aic-metrics="${metrics}")
            fi
        fi
    fi

    local rc=0
    local wall="${MSPROF_WALL_TIMEOUT_SEC:-1800}"
    local t0 t1 wall_sec
    t0="$(date +%s.%N 2>/dev/null || date +%s)"
    _msprof_exec() {
        if [ "${wall}" != "0" ] && command -v timeout >/dev/null 2>&1; then
            timeout --foreground "${wall}" "$@"
        else
            "$@"
        fi
    }
    if ! _msprof_exec "${cmd[@]}"; then
        rc=$?
    fi
    # npu app 模式若本机 msprof 不认 --application，回退 op（仍解析 csv；并警告）
    if [ "${rc}" -ne 0 ] && [ "${mode}" = "npu" ] && [ "${MSPROF_MODE:-app}" != "op" ]; then
        echo "[msprof_run] WARN: app+aic-metrics 退出 ${rc}，重试不带 --aic-metrics" >&2
        cmd=(msprof --application="${app_cmd:-${bin}}" --output="${prof_out}")
        rc=0
        _msprof_exec "${cmd[@]}" || rc=$?
    fi
    if [ "${rc}" -ne 0 ] && [ "${mode}" = "npu" ] && [ "${MSPROF_MODE:-app}" != "op" ]; then
        echo "[msprof_run] WARN: app 模式仍失败 rc=${rc}，回退 msprof op（请核对 csv 是否覆盖全部 launch）" >&2
        cmd=(
            msprof op "${bin}"
            --output="${prof_out}"
            --aic-metrics="${MSPROF_AIC_METRICS_NPU:-PipeUtilization,MemoryUB,Memory}"
            --launch-count="${MSPROF_LAUNCH_COUNT_NPU:-64}"
        )
        cmd+=("$@")
        rc=0
        _msprof_exec "${cmd[@]}" || rc=$?
    fi
    t1="$(date +%s.%N 2>/dev/null || date +%s)"
    wall_sec="$(awk -v s="${t0}" -v e="${t1}" 'BEGIN { printf "%.3f", (e - s) + 0 }')"
    echo "[wall_sec] ${wall_sec}"
    echo "[msprof_run] wall_sec=${wall_sec} rc=${rc}"
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
        python3 "${_MSPROF_RUN_REPO_ROOT}/scripts/npu_msprof_summarize.py" \
            --prof "${prof_out}" \
            --jsonl "${NPU_LAUNCH_METRICS_FILE:-$(pwd)/output/npu_launch_metrics.jsonl}" \
            "$(pwd)" || true
    fi
}
