#!/usr/bin/env bash
# npu_kem_textbook_perf.sh — 教材 KEM 实机性能 + profiling（表 A 14 档）。
#
# 清单：docs/research/教材KEM实机测量清单.md
# 路径表：scripts/npu_kem_textbook_list.txt
#
# 默认：
#   RUN_WITH_MSPROF=1  MSPROF_MODE=app   — 整进程一次采集，kernel_details 覆盖全部 launch
#   分卡：npu_device_map（stable=1 / examples=2 / tests=3）
#   SKIP_L18_RISK=0   — 教材就是要 Encaps/Decaps 真值；timeout 124 后同卡勿连环重跑
#
# 用法（仓库根，先 unset ASCEND_DEVICE_ID）：
#   NPU_SUITE_DRY_RUN=1 bash scripts/npu_kem_textbook_perf.sh     # 只打印分卡
#   bash scripts/npu_kem_textbook_perf.sh                        # 实机 14 档
#   TEXTBOOK_ONLY=1,2,5 bash scripts/npu_kem_textbook_perf.sh    # 只跑编号
#   TEXTBOOK_INCLUDE_1024_INCUBATING=1 …                         # 再加 1024 incubating 四档
#   SKIP_L18_RISK=1 …                                            # 只跑 KeyGen（1/5/9）
#
# 每档产物：
#   output/npu_launch_metrics.jsonl     host 逐 launch
#   output/run_metrics.txt              含 [npu_launch] 与 [wall_sec]
#   prof_npu/<bin>/                     msprof OPPROF_* / kernel_details.csv
# 汇总：python3 scripts/npu_msprof_summarize.py <用例>

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_CASE="${REPO_ROOT}/scripts/npu_run_case.sh"
LIST="${REPO_ROOT}/scripts/npu_kem_textbook_list.txt"
STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_ROOT="${REPO_ROOT}/output/npu_textbook/${STAMP}"
mkdir -p "${LOG_ROOT}"

export SOC_VERSION="${SOC_VERSION:-Ascend910B4}"
export RUN_WITH_MSPROF="${RUN_WITH_MSPROF:-1}"
export MSPROF_MODE="${MSPROF_MODE:-app}"
export MSPROF_LAUNCH_COUNT_NPU="${MSPROF_LAUNCH_COUNT_NPU:-64}"
SKIP_L18_RISK="${SKIP_L18_RISK:-0}"
STOP_ON_FAIL="${NPU_SUITE_STOP_ON_FAIL:-1}"
FORCE_RB="${NPU_SUITE_FORCE_REBUILD:-1}"
DRY="${NPU_SUITE_DRY_RUN:-0}"

echo "[textbook] log=${LOG_ROOT} DRY=${DRY} MSPROF_MODE=${MSPROF_MODE} SKIP_L18_RISK=${SKIP_L18_RISK}"
if [ "${DRY}" != "1" ]; then
    exec >>"${LOG_ROOT}/suite.log" 2>&1
fi
echo "[textbook] start stamp=${STAMP} device=${ASCEND_DEVICE_ID:-<per-case map>}"

_is_l18() {
    local p="$1"
    case "${p}" in
    *encaps* | *decaps*) return 0 ;;
    *) return 1 ;;
    esac
}

_run_one() {
    local id="$1"
    local rel="$2"
    local dir="${REPO_ROOT}/${rel}"
    if [ ! -f "${dir}/run.sh" ]; then
        echo "[textbook] ERROR: missing ${dir}/run.sh" >&2
        return 1
    fi
    if [ "${SKIP_L18_RISK}" = "1" ] && _is_l18 "${rel}"; then
        echo "[textbook] SKIP #${id} l18-risk ${rel}"
        return 0
    fi
    local args=(--dir "${dir}" --label "T${id}_$(basename "${dir}")" --msprof)
    if [ "${FORCE_RB}" = "1" ]; then
        args+=(--force-rebuild)
    fi
    if [ "${DRY}" = "1" ]; then
        args+=(--dry-run)
    fi
    echo "[textbook] === #${id} ${rel} ==="
    if ! bash "${RUN_CASE}" "${args[@]}"; then
        echo "[textbook] FAIL #${id}" >&2
        if [ "${STOP_ON_FAIL}" = "1" ]; then
            echo "[textbook] STOP_ON_FAIL=1；timeout 124 则同卡勿重跑 Encaps/Decaps，见 npu_card_guard" >&2
            exit 1
        fi
        return 1
    fi
    if [ "${DRY}" != "1" ]; then
        python3 "${REPO_ROOT}/scripts/npu_msprof_summarize.py" "${dir}" | tee -a "${LOG_ROOT}/summary.txt" || true
    fi
}

ONLY="${TEXTBOOK_ONLY:-}"
declare -A only_map=()
if [ -n "${ONLY}" ]; then
    IFS=',' read -r -a _ids <<<"${ONLY}"
    for i in "${_ids[@]}"; do
        only_map["${i}"]=1
    done
fi

while IFS=$'\t' read -r id rel || [ -n "${id:-}" ]; do
    [[ -z "${id}" || "${id}" == \#* ]] && continue
    if [ -n "${ONLY}" ] && [ -z "${only_map[${id}]+x}" ]; then
        continue
    fi
    _run_one "${id}" "${rel}"
done <"${LIST}"

if [ "${TEXTBOOK_INCLUDE_1024_INCUBATING:-0}" = "1" ]; then
    _run_one "15" "examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-keygen-k4"
    _run_one "16" "examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-encaps-k4"
    _run_one "17" "examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-decaps-k4"
    _run_one "18" "examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-decaps-ct-k4"
fi

echo "[textbook] done stamp=${STAMP} summary=${LOG_ROOT}/summary.txt"
echo "[textbook] 填表：优先 [msprof_kernel_total] / [msprof_kernel]；host 对照 [npu_launch_total]"
