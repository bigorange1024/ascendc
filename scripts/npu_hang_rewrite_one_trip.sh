#!/usr/bin/env bash
# npu_hang_rewrite_one_trip.sh — Encrypt 卡死重写：一次搬码跑全诊断套件
#
# ★ 反馈硬约束：用户无法回传任何文件，只能打字。
#   跑完打印 TYPE_BACK；把「ID 状态 编号…」打回聊天即可。
#   禁止要求 FEEDBACK.md / BRING_BACK.tar.gz / 日志包。
#
# 用法（仓库根）：
#   NPU_HANG_MANIFEST=1 bash scripts/npu_hang_rewrite_one_trip.sh
#   bash scripts/npu_hang_rewrite_one_trip.sh
#
# 环境：unset ASCEND_DEVICE_ID；SOC_VERSION=Ascend910B4
# 说明：docs/engineering/Encrypt卡死重写-实机一次测清单.md

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_CASE="${REPO_ROOT}/scripts/npu_run_case.sh"

STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_ROOT="${REPO_ROOT}/output/npu_hang_rewrite/${STAMP}"
mkdir -p "${LOG_ROOT}/cases" "${LOG_ROOT}/snippets" "${LOG_ROOT}/trace_maps"

export SOC_VERSION="${SOC_VERSION:-Ascend910B4}"
export NPU_RUN_LOG_ROOT="${LOG_ROOT}/npu_runs"
export F203_L18_TRACE="${F203_L18_TRACE:-1}"

CONTINUE="${NPU_HANG_CONTINUE:-1}"
TOY_TO="${NPU_HANG_TOY_TIMEOUT:-180}"
PROD_TO="${NPU_HANG_PROD_TIMEOUT:-600}"
SKIP_TOYS="${NPU_HANG_SKIP_TOYS:-0}"
SKIP_PROD="${NPU_HANG_SKIP_PROD:-0}"
MANIFEST="${NPU_HANG_MANIFEST:-0}"
DRY="${NPU_SUITE_DRY_RUN:-0}"

# 用户若已显式 export ASCEND_DEVICE_ID，整趟尊重；否则每例按 device_map 重设（勿让 N0 的卡1粘到 toys）
USER_SET_DEVICE=0
if [ -n "${ASCEND_DEVICE_ID+x}" ]; then
    USER_SET_DEVICE=1
fi

STABLE="${REPO_ROOT}/examples/stable/ml-kem/ml-kem-1024"
TOYS="${REPO_ROOT}/graph-tests/toys"

RESULTS="${LOG_ROOT}/RESULTS.tsv"
TYPE_BACK="${LOG_ROOT}/TYPE_BACK.txt"
: >"${RESULTS}"
echo -e "id\tlabel\trc\ttimeout_s\tlog" >>"${RESULTS}"

_log() { printf '%s\n' "$*" | tee -a "${LOG_ROOT}/one_trip.log"; }

_device_id() {
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/npu_device_map.sh"
    npu_device_id_for_path "$1"
}

_die_missing() {
    _log "ERROR: missing case dir: $1"
    exit 2
}

_extract_trace() {
    local logf="$1"
    local outf="$2"
    if [ ! -f "${logf}" ]; then
        echo "(no log)" >"${outf}"
        return
    fi
    {
        echo "### grep TRACE / l18-trace / SUCCESS / FAIL / budget"
        grep -E '^\[l18-trace\]|^\[SUCCESS\]|^\[FAIL\]|budget|kernel-run-timeout|stages set=|^[0-9]{2,3}( [0-9]{2,3})*$' \
            "${logf}" 2>/dev/null | tail -n 80 || true
    } >"${outf}"
}

_codes_from_snippet() {
    local snippet="$1"
    [ -f "${snippet}" ] || return 0
    local l18
    l18="$(grep -E '\[l18-trace\]' "${snippet}" 2>/dev/null | tail -n 1 || true)"
    if [ -n "${l18}" ]; then
        echo "${l18}" | sed -n 's/.*: *//p' | grep -Eo '[0-9]+' | tr '\n' ' ' | sed 's/ $//'
        return 0
    fi
    grep -E '^[0-9]{2,3}$' "${snippet}" 2>/dev/null | tr '\n' ' ' | sed 's/ $//' || true
}

_why_from_log() {
    # 从用例日志抽一行「可打字」原因；无则空
    local logf="$1"
    [ -f "${logf}" ] || return 0
    # 优先 ERROR / FAIL / preflight / cmake / ACL / npu
    local line
    line="$(grep -E 'ERROR|error:|\[FAIL\]|preflight|cmake|ACL|npu-smi|npu_guard|undefined|No such|not found|Permission|Floating point' \
        "${logf}" 2>/dev/null | grep -v '^###' | tail -n 1 || true)"
    if [ -z "${line}" ]; then
        line="$(tail -n 3 "${logf}" 2>/dev/null | tr '\n' ' ' | sed 's/  */ /g' || true)"
    fi
    # 压成单行、截断，方便打字
    echo "${line}" | tr '\t' ' ' | sed 's/  */ /g' | cut -c1-160
}

_append_type_line() {
    local id="$1"
    local label="$2"
    local rc="$3"
    local snippet="$4"
    local case_log="${5:-}"
    local status
    # 状态词：通 / 超时(124) / 失败(秒退或编译/ACL 等非超时) / 清单
    # 「失败」≠ SynchronizeStream 卡死；真卡死通常是 超时
    case "${rc}" in
    0) status="通" ;;
    124) status="超时" ;;
    MANIFEST) status="清单" ;;
    *) status="失败" ;;
    esac
    local codes
    codes="$(_codes_from_snippet "${snippet}")"
    local why=""
    if [ "${rc}" != "0" ] && [ "${rc}" != "MANIFEST" ]; then
        why="$(_why_from_log "${case_log}")"
    fi
    if [ -n "${why}" ]; then
        printf '%s %s rc=%s why=%s\n' "${id}" "${status}" "${rc}" "${why}" >>"${TYPE_BACK}"
        _log ">>> TYPE: ${id} ${status} rc=${rc} why=${why}"
    elif [ -z "${codes}" ]; then
        printf '%s %s rc=%s\n' "${id}" "${status}" "${rc}" >>"${TYPE_BACK}"
        _log ">>> TYPE: ${id} ${status} rc=${rc}   # ${label}"
    else
        printf '%s %s %s\n' "${id}" "${status}" "${codes}" >>"${TYPE_BACK}"
        _log ">>> TYPE: ${id} ${status} ${codes}   # ${label}"
    fi
}

_init_type_back() {
    cat >"${TYPE_BACK}" <<EOF
========== 请只打字回传（不要传任何文件）==========
跑次：${STAMP}
每行：ID 状态 rc=… [why=…] 或 [编号…]
状态：通 | 失败(秒退/编译/ACL…) | 超时(真像卡死) | 清单
失败时终端会刷 device=、日志末 30 行、why=；把 why 或末行关键字打回即可（不要传文件）。
EOF
}

_run_one() {
    local id="$1"
    local label="$2"
    local dir="$3"
    local tmo="$4"
    shift 4
    local extra_env=("$@")

    if [ ! -f "${dir}/run.sh" ]; then
        _die_missing "${dir}"
    fi

    local case_log="${LOG_ROOT}/cases/${id}_${label}.log"
    local snippet="${LOG_ROOT}/snippets/${id}_${label}_trace.txt"

    local device
    device="$(_device_id "${dir}")"

    if [ "${MANIFEST}" = "1" ]; then
        _log "[MANIFEST] ${id} ${label} dir=${dir} timeout=${tmo}s device=${device} env=[${extra_env[*]}]"
        echo -e "${id}\t${label}\tMANIFEST\t${tmo}\t-" >>"${RESULTS}"
        _append_type_line "${id}" "${label}" "MANIFEST" "/dev/null" ""
        return 0
    fi

    _log "=== ${id} ${label} (timeout=${tmo}s device=${device}) ==="
    # 未显式指定时，每例按树分卡（stable=1 / graph-tests=3）
    if [ "${USER_SET_DEVICE}" != "1" ]; then
        export ASCEND_DEVICE_ID="${device}"
    fi
    _log "[device] ASCEND_DEVICE_ID=${ASCEND_DEVICE_ID} (map=${device} user_set=${USER_SET_DEVICE})"
    local rc=0
    set +e
    # tee：屏幕与 case_log 同步，避免「只见 FAIL、看不到原因」（打字反馈刚需）
    if [ "${DRY}" = "1" ]; then
        env "${extra_env[@]}" bash "${RUN_CASE}" --dir "${dir}" --label "${label}" --force-rebuild --dry-run \
            2>&1 | tee "${case_log}"
        rc=${PIPESTATUS[0]}
    else
        env "${extra_env[@]}" timeout --signal=KILL "${tmo}" \
            bash "${RUN_CASE}" --dir "${dir}" --label "${label}" --force-rebuild \
            2>&1 | tee "${case_log}"
        rc=${PIPESTATUS[0]}
    fi
    set -e

    _extract_trace "${case_log}" "${snippet}"
    if [ -d "${NPU_RUN_LOG_ROOT}" ]; then
        local latest
        latest="$(ls -dt "${NPU_RUN_LOG_ROOT}"/*_"${label}" 2>/dev/null | head -1 || true)"
        if [ -n "${latest}" ] && [ -f "${latest}/run.log" ]; then
            _extract_trace "${latest}/run.log" "${snippet}.run_case.txt"
            cat "${snippet}.run_case.txt" >>"${snippet}" || true
        fi
    fi

    echo -e "${id}\t${label}\t${rc}\t${tmo}\t${case_log}" >>"${RESULTS}"
    if [ "${rc}" -eq 0 ]; then
        _log "[PASS] ${id} ${label} device=${device}"
    elif [ "${rc}" -eq 124 ]; then
        _log "[TIMEOUT] ${id} ${label} after ${tmo}s device=${device} — hang candidate"
        _log "----- ${id} 日志末 30 行（请打字抄关键句）-----"
        tail -n 30 "${case_log}" 2>/dev/null | tee -a "${LOG_ROOT}/one_trip.log" || true
        _log "----- end -----"
    else
        _log "[FAIL] ${id} ${label} rc=${rc} device=${device}"
        _log "----- ${id} 日志末 30 行（请打字抄关键句）-----"
        tail -n 30 "${case_log}" 2>/dev/null | tee -a "${LOG_ROOT}/one_trip.log" || true
        _log "----- end -----"
    fi

    _append_type_line "${id}" "${label}" "${rc}" "${snippet}" "${case_log}"

    if [ "${rc}" -ne 0 ] && [ "${CONTINUE}" != "1" ]; then
        return "${rc}"
    fi
    return 0
}

_print_type_back() {
    _log ""
    _log "######################################################################"
    _log "# 反馈方式：只打字。不要传 tar / md / 日志 / 截图。"
    _log "######################################################################"
    tee -a "${LOG_ROOT}/one_trip.log" <"${TYPE_BACK}"
    _log "========== TYPE_BACK 结束 =========="
}

_copy_trace_maps() {
    local dest="$1"
    mkdir -p "${dest}"
    local t d
    for t in T01 T02 T03 T04 T05 T06 T07; do
        d=$(echo "${TOYS}/${t}"*)
        if [ -f "${d}/trace_map.md" ]; then
            cp -f "${d}/trace_map.md" "${dest}/${t}_trace_map.md"
        fi
    done
}

_log "LOG_ROOT=${LOG_ROOT}"
_log "F203_L18_TRACE=${F203_L18_TRACE} FORCE_REBUILD=1 CONTINUE=${CONTINUE} MANIFEST=${MANIFEST} DRY=${DRY}"
_log "反馈模式：只打字（TYPE_BACK），不要求回传文件"

_init_type_back
_copy_trace_maps "${LOG_ROOT}/trace_maps"

_run_one N0 pke_keygen "${STABLE}/stable-fips203-mlkem-pke-keygen-k4" "${PROD_TO}" || true

if [ "${SKIP_TOYS}" != "1" ]; then
    _run_one N1 T01_mix13 "${TOYS}/T01-mix-ntt13-handshake" "${TOY_TO}" || true
    _run_one N2 T02_gate "${TOYS}/T02-prod-gate-timing" "${TOY_TO}" || true
    _run_one N3 T03_fsm "${TOYS}/T03-full-fsm-ntt-gate-intt" "${TOY_TO}" || true
    _run_one N4 T04_vol "${TOYS}/T04-gate-volume-stress" "${TOY_TO}" || true
    _run_one N5 T05_2launch "${TOYS}/T05-multi-launch-rounds" "${TOY_TO}" || true
    _run_one N6 T06_mac "${TOYS}/T06-gate-real-brick" "${TOY_TO}" || true
    _run_one N7 T07_sample "${TOYS}/T07-sampling-then-fsm" "${TOY_TO}" || true
fi

if [ "${SKIP_PROD}" != "1" ]; then
    _run_one N8 pke_encrypt "${STABLE}/stable-fips203-mlkem-pke-encrypt-k4" "${PROD_TO}" \
        F203_L18_TRACE=1 ENCRYPT_FORCE_REBUILD=1 || true
    _run_one N9 kem_encaps "${STABLE}/stable-fips203-mlkem-kem-encaps-k4" "${PROD_TO}" \
        F203_L18_TRACE=1 KEM_ENCAPS_FORCE_REBUILD=1 || true
    _run_one N10 pke_decrypt "${STABLE}/stable-fips203-mlkem-pke-decrypt-k4" "${PROD_TO}" \
        DECRYPT_FORCE_REBUILD=1 || true
fi

{
    echo "# npu_hang_rewrite ${STAMP}"
    echo
    echo "| ID | label | rc | note |"
    echo "|----|-------|----|------|"
    tail -n +2 "${RESULTS}" | while IFS=$'\t' read -r id label rc tmo logp; do
        note="ok"
        [ "${rc}" = "124" ] && note="TIMEOUT/hang?"
        [ "${rc}" != "0" ] && [ "${rc}" != "124" ] && [ "${rc}" != "MANIFEST" ] && note="FAIL"
        [ "${rc}" = "MANIFEST" ] && note="manifest-only"
        echo "| ${id} | ${label} | ${rc} | ${note} |"
    done
} | tee "${LOG_ROOT}/STATUS.md"

ln -sfn "${LOG_ROOT}" "${REPO_ROOT}/output/npu_hang_rewrite/latest" 2>/dev/null || true

_print_type_back

_log "DONE. 请把上面 TYPE_BACK 整段打回聊天（不要传文件）。"
_log "latest -> output/npu_hang_rewrite/latest"
_log "说明 -> docs/engineering/Encrypt卡死重写-实机一次测清单.md"
