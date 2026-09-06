#!/usr/bin/env bash
# npu_hang_rewrite_one_trip.sh — Encrypt 卡死重写：一次搬码跑全诊断套件
#
# 用户在实机只需执行本脚本；单例挂死用 timeout 隔离，不拖死整趟；
# 结束产出 BRING_BACK.tar.gz + FEEDBACK.md，填编号回传即可。
#
# 用法（仓库根）：
#   NPU_HANG_MANIFEST=1 bash scripts/npu_hang_rewrite_one_trip.sh
#   NPU_SUITE_DRY_RUN=1 bash scripts/npu_hang_rewrite_one_trip.sh
#   bash scripts/npu_hang_rewrite_one_trip.sh
#
# 环境：
#   unset ASCEND_DEVICE_ID     # 推荐；按 npu_device_map（stable=1, graph-tests=3）
#   SOC_VERSION=Ascend910B4
#   NPU_HANG_CONTINUE=1
#   NPU_HANG_TOY_TIMEOUT=180
#   NPU_HANG_PROD_TIMEOUT=600
#   NPU_HANG_SKIP_TOYS=0
#   NPU_HANG_SKIP_PROD=0
#   F203_L18_TRACE=1
#
# 日志：output/npu_hang_rewrite/<stamp>/
# 带回：…/BRING_BACK.tar.gz + FEEDBACK.md
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

STABLE="${REPO_ROOT}/examples/stable/ml-kem/ml-kem-1024"
TOYS="${REPO_ROOT}/graph-tests/toys"

RESULTS="${LOG_ROOT}/RESULTS.tsv"
FEEDBACK="${LOG_ROOT}/FEEDBACK.md"
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

    if [ "${MANIFEST}" = "1" ]; then
        _log "[MANIFEST] ${id} ${label} dir=${dir} timeout=${tmo}s device=$(_device_id "${dir}") env=[${extra_env[*]}]"
        echo -e "${id}\t${label}\tMANIFEST\t${tmo}\t-" >>"${RESULTS}"
        return 0
    fi

    _log "=== ${id} ${label} (timeout=${tmo}s) ==="
    local rc=0
    set +e
    if [ "${DRY}" = "1" ]; then
        env "${extra_env[@]}" bash "${RUN_CASE}" --dir "${dir}" --label "${label}" --force-rebuild --dry-run \
            >"${case_log}" 2>&1
        rc=$?
    else
        env "${extra_env[@]}" timeout --signal=KILL "${tmo}" \
            bash "${RUN_CASE}" --dir "${dir}" --label "${label}" --force-rebuild \
            >"${case_log}" 2>&1
        rc=$?
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
        _log "[PASS] ${id} ${label}"
    elif [ "${rc}" -eq 124 ]; then
        _log "[TIMEOUT] ${id} ${label} after ${tmo}s — hang candidate; see ${snippet}"
    else
        _log "[FAIL] ${id} ${label} rc=${rc} — see ${case_log}"
    fi

    if [ "${rc}" -ne 0 ] && [ "${CONTINUE}" != "1" ]; then
        return "${rc}"
    fi
    return 0
}

_write_feedback_skel() {
    cat >"${FEEDBACK}" <<EOF
# Encrypt 卡死重写 — 实机反馈单（一次搬码）

**跑次**：\`${STAMP}\`
**回传**：填本表 + \`BRING_BACK.tar.gz\` 交给 Agent。

## 怎么填（最少）

\`状态\` = 通 / 挂 / 超时 / 编译失败；\`TRACE\` = 编号序列或 \`[l18-trace]\` 下标。

| ID | 用例 | 状态 | TRACE |
|----|------|------|-------|
| N0 | PKE KeyGen（对照） |  |  |
| N1 | T01 最短 MIX 1/3 |  |  |
| N2 | T02 生产 GATE 时序 |  |  |
| N3 | T03 全 FSM |  |  |
| N4 | T04 假循环×10 |  |  |
| N5 | T05 2×launch |  |  |
| N6 | T06 真 Vec MAC |  |  |
| N7 | T07 SAMPLE→FSM |  |  |
| N8 | PKE Encrypt + F203_L18_TRACE |  |  |
| N9 | KEM Encaps + F203_L18_TRACE |  |  |
| N10 | PKE Decrypt（可选对照） |  |  |

## TRACE 怎么读（两套编号，勿混）

### A. toys（T01–T07）
Host 十进制编号，见 \`BRING_BACK/trace_maps/\`。缺号=未到达。T01 收尾应为 \`199\`。

### B. Encrypt / Encaps（\`F203_L18_TRACE=1\`）
\`[l18-trace] stages set=K/16 : 0 1 2 …\`

| 下标 | 含义 |
|------|------|
| 0 | AIV NTT SPLIT |
| 1 | AIC NTT MMAD |
| 2 | AIV NTT YHAT |
| 3 | AIV AT_JP START |
| 4 | AIV AT_JP DONE |
| 5 | AIV IP SIGNAL |
| 6 | AIC IP WAIT DONE |
| 7 | AIV INTT SPLIT |
| 8 | AIC INTT MMAD |
| 9 | AIV INTT U |
| 10 | AIV E1 DONE |
| 11 | AIC AT_JP GATE |
| 12 | AIV AT_JP GATE |
| 13 | AIV DECODE T |
| 14 | AIV V DONE |
| 15 | AIV MU E2 |

GATE 卡死常见停在 3–6 / 11–12。设备全空：launch/进核问题。

## 决策树（Agent 用）

- N0 挂 → 整卡/环境
- N0 绿、N1 挂 → MIX NPU
- N2 挂、N1 绿 → GATE/Wait(4)
- toys 全绿、N8 挂 → 生产体量；按 l18-trace 开 enc_related
- N8/N9 同形态 → 并案 Encaps=Encrypt 核
EOF
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

_pack_bring_back() {
    local out="${LOG_ROOT}/BRING_BACK"
    rm -rf "${out}"
    mkdir -p "${out}/logs" "${out}/snippets" "${out}/cases" "${out}/trace_maps"
    cp -f "${LOG_ROOT}/STATUS.md" "${LOG_ROOT}/RESULTS.tsv" "${LOG_ROOT}/FEEDBACK.md" "${out}/logs/" 2>/dev/null || true
    cp -f "${LOG_ROOT}/one_trip.log" "${out}/logs/" 2>/dev/null || true
    cp -f "${LOG_ROOT}/snippets/"* "${out}/snippets/" 2>/dev/null || true
    for f in "${LOG_ROOT}/cases/"*.log; do
        [ -f "${f}" ] || continue
        tail -n 200 "${f}" >"${out}/cases/$(basename "${f}").tail.txt"
    done
    _copy_trace_maps "${out}/trace_maps"
    if [ -f "${REPO_ROOT}/docs/engineering/Encrypt卡死重写-实机一次测清单.md" ]; then
        cp -f "${REPO_ROOT}/docs/engineering/Encrypt卡死重写-实机一次测清单.md" "${out}/logs/"
    fi
    if [ -f "${REPO_ROOT}/docs/engineering/Encrypt卡死重写-sync清单.txt" ]; then
        cp -f "${REPO_ROOT}/docs/engineering/Encrypt卡死重写-sync清单.txt" "${out}/logs/"
    fi
    tar -C "${LOG_ROOT}" -czf "${LOG_ROOT}/BRING_BACK.tar.gz" BRING_BACK
    _log "BRING_BACK=${LOG_ROOT}/BRING_BACK.tar.gz"
}

_log "LOG_ROOT=${LOG_ROOT}"
_log "F203_L18_TRACE=${F203_L18_TRACE} FORCE_REBUILD=1 CONTINUE=${CONTINUE} MANIFEST=${MANIFEST} DRY=${DRY}"

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

_write_feedback_skel

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
    echo
    echo "FEEDBACK: \`${FEEDBACK}\`"
    echo "RESULTS: \`${RESULTS}\`"
} | tee "${LOG_ROOT}/STATUS.md"

ln -sfn "${LOG_ROOT}" "${REPO_ROOT}/output/npu_hang_rewrite/latest" 2>/dev/null || true

_pack_bring_back

_log "DONE. 填 FEEDBACK.md 后连同 BRING_BACK.tar.gz 回传即可。"
_log "latest -> output/npu_hang_rewrite/latest"
_log "说明 -> docs/engineering/Encrypt卡死重写-实机一次测清单.md"
