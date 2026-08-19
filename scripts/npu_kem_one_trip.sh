#!/usr/bin/env bash
# npu_kem_one_trip.sh — 实机「一次搬码、尽量测全」主入口（2026-08-19）。
#
# 背景：借入实机搬码成本高；本脚本把 l18 诊断、1024 验收、教材 14 档性能、探针冒烟
# 合成一条命令，按分卡顺序跑，失败时尽量继续非卡-1-l18 项，最后打包可带回日志。
#
# 用法（仓库根；CANN 由 run.sh 内 source env.sh）：
#   NPU_ONE_TRIP_MANIFEST=1 bash scripts/npu_kem_one_trip.sh   # 只打印将跑什么（无卡可跑）
#   NPU_SUITE_DRY_RUN=1 bash scripts/npu_kem_one_trip.sh        # 分卡 dry-run
#   bash scripts/npu_kem_one_trip.sh                             # 实机一次跑全（默认）
#
# 环境：
#   unset ASCEND_DEVICE_ID          # 推荐；分卡 stable=1 / examples=2 / tests=3
#   SOC_VERSION=Ascend910B4
#   ONE_TRIP_INCLUDE_1024_INCUBATING=0   # 1=追加 1024 incubating 四档（表 B）
#   ONE_TRIP_SKIP_TEXTBOOK=0             # 1=跳过教材 14 档 msprof（仅诊断+验收）
#   ONE_TRIP_SKIP_PROBES=0               # 1=跳过 alg13/14/15/19 探针
#   ONE_TRIP_CONTINUE_ON_L18_FAIL=1      # 默认 1：E1 失败后仍跑 768/512/探针
#   NPU_SUITE_STOP_ON_FAIL=0             # 默认 0：单步失败不中止整趟（E0 失败除外）
#   NPU_SUITE_FORCE_REBUILD=1
#
# 日志：output/npu_one_trip/<stamp>/one_trip.log
# 带回：output/npu_one_trip/<stamp>/BRING_BACK.tar.gz（见 npu_kem_collect_artifacts.sh）

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SUITE="${REPO_ROOT}/scripts/npu_kem_real_machine_suite.sh"
TEXTBOOK="${REPO_ROOT}/scripts/npu_kem_textbook_perf.sh"
COLLECT="${REPO_ROOT}/scripts/npu_kem_collect_artifacts.sh"
RUN_CASE="${REPO_ROOT}/scripts/npu_run_case.sh"

STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_ROOT="${REPO_ROOT}/output/npu_one_trip/${STAMP}"
mkdir -p "${LOG_ROOT}"

export SOC_VERSION="${SOC_VERSION:-Ascend910B4}"
export SKIP_L18_RISK=0
export NPU_SUITE_FORCE_REBUILD="${NPU_SUITE_FORCE_REBUILD:-1}"
export NPU_RUN_LOG_ROOT="${LOG_ROOT}/npu_runs"
export NPU_SUITE_STOP_ON_FAIL="${NPU_SUITE_STOP_ON_FAIL:-0}"
CONTINUE_L18="${ONE_TRIP_CONTINUE_ON_L18_FAIL:-1}"
INCLUDE_1024_INC="${ONE_TRIP_INCLUDE_1024_INCUBATING:-0}"
SKIP_TEXTBOOK="${ONE_TRIP_SKIP_TEXTBOOK:-0}"
SKIP_PROBES="${ONE_TRIP_SKIP_PROBES:-0}"
MANIFEST="${NPU_ONE_TRIP_MANIFEST:-0}"
DRY="${NPU_SUITE_DRY_RUN:-0}"

STABLE="${REPO_ROOT}/examples/stable/ml-kem/ml-kem-1024"
S_PKE_KG="${STABLE}/stable-fips203-mlkem-pke-keygen-k4"
S_ENCRYPT="${STABLE}/stable-fips203-mlkem-pke-encrypt-k4"
S_DECRYPT="${STABLE}/stable-fips203-mlkem-pke-decrypt-k4"
S_KG="${STABLE}/stable-fips203-mlkem-kem-keygen-k4"

L18_DIAG_FAIL=0
E0_FAIL=0

_status() {
    printf '%s\n' "$*" | tee -a "${LOG_ROOT}/STATUS.md"
}

_invoke_suite() {
    local phase="$1"
    export NPU_SUITE_PHASE="${phase}"
    export NPU_SUITE_DRY_RUN="${DRY}"
    if [ "${MANIFEST}" = "1" ]; then
        echo "[one_trip] MANIFEST suite PHASE=${phase}"
        return 0
    fi
    echo "[one_trip] === suite PHASE=${phase} ==="
    if ! bash "${SUITE}"; then
        return 1
    fi
    return 0
}

_run_case() {
    local label="$1"
    local dir="$2"
    shift 2
    local extra_env=("$@")
    local args=(--dir "${dir}" --label "${label}")
    if [ "${NPU_SUITE_FORCE_REBUILD}" = "1" ]; then
        args+=(--force-rebuild)
    fi
    if [ "${DRY}" = "1" ]; then
        args+=(--dry-run)
    fi
    if [ "${MANIFEST}" = "1" ]; then
        echo "[one_trip] MANIFEST case ${label} ${dir} env=[${extra_env[*]}]"
        return 0
    fi
    echo "[one_trip] === case ${label} ==="
    if ! env "${extra_env[@]}" bash "${RUN_CASE}" "${args[@]}"; then
        return 1
    fi
    return 0
}

_invoke_textbook() {
    local only="${1:-}"
    export NPU_SUITE_DRY_RUN="${DRY}"
    export SKIP_L18_RISK=0
    export RUN_WITH_MSPROF=1
    export MSPROF_MODE="${MSPROF_MODE:-app}"
    if [ -n "${only}" ]; then
        export TEXTBOOK_ONLY="${only}"
    else
        unset TEXTBOOK_ONLY 2>/dev/null || true
    fi
    if [ "${MANIFEST}" = "1" ]; then
        echo "[one_trip] MANIFEST textbook ONLY=${only:-all}"
        return 0
    fi
    echo "[one_trip] === textbook ONLY=${only:-all} ==="
    export NPU_RUN_LOG_ROOT="${LOG_ROOT}/npu_runs"
    if ! bash "${TEXTBOOK}"; then
        return 1
    fi
    return 0
}

_print_manifest() {
    cat <<EOF
[npu_one_trip] 一次搬码计划（约 ${STAMP}）

搬码前（办公室 / Cloud，无 NPU 也可）：
  NPU_SUITE_DRY_RUN=1 bash scripts/npu_kem_one_trip.sh

实机（仓库根，unset ASCEND_DEVICE_ID）：
  bash scripts/npu_kem_one_trip.sh

分卡：stable→1  examples(768/512)→2  ascendc-tests→3

块序（故意把 l18 诊断放在教材 Encaps 性能之前，避免白跑）：
  A  三包卡 preflight（npu-smi Process 段）
  B  卡1 安全基线：E0 smoke + stable PKE×3 + KEM KeyGen(FORCE_REBUILD)
  C  卡1 l18 诊断：E1 Encaps+F203_L18_TRACE=1 → E2 同卡再跑 Encaps
  D  卡1 集成（仅 C 成功）：roundtrip + msprof Decrypt/D-E/全链 Decaps
  E  教材 msprof：#1–4 卡1（C 失败则只跑 #1 KeyGen）；#5–12 卡2；#13–14 卡3
  F  卡3 探针 alg13/14/15/19（不含 alg20/21 device 探针）
  G  可选 1024 incubating 四档（ONE_TRIP_INCLUDE_1024_INCUBATING=1）
  H  打包 BRING_BACK.tar.gz

E1 失败仍继续（默认）：768/512/探针/correctness 仍跑；卡1 跳过 Encaps/Decaps 性能重复项。

必带回：output/npu_one_trip/latest/BRING_BACK.tar.gz 与 STATUS.md

EOF
}

if [ "${MANIFEST}" = "1" ]; then
    _print_manifest
    exit 0
fi

if [ "${DRY}" != "1" ]; then
    exec >>"${LOG_ROOT}/one_trip.log" 2>&1
fi

_print_manifest | tee -a "${LOG_ROOT}/STATUS.md"
_status ""
_status "# 实机一次搬码 STATUS"
_status ""
_status "| 时间 | ${STAMP} |"
_status "| SOC | ${SOC_VERSION} |"
_status "| DRY | ${DRY} |"
_status ""

# --- A: preflight 三包卡 ---
if [ "${DRY}" != "1" ] && [ "${MANIFEST}" != "1" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/npu_card_guard.sh"
    for dev in 1 2 3; do
        echo "[one_trip] preflight device=${dev}"
        if ! ASCEND_DEVICE_ID="${dev}" npu_card_guard_preflight >>"${LOG_ROOT}/preflight_dev${dev}.log" 2>&1; then
            _status "| preflight dev${dev} | WARN（见 preflight_dev${dev}.log；可 NPU_GUARD_STRICT=0 继续） |"
        else
            _status "| preflight dev${dev} | OK |"
        fi
    done
fi

# --- B: 卡1 安全基线 ---
if ! _invoke_suite e0; then
    E0_FAIL=1
    _status "| B E0 smoke | **FAIL** — 环境/卡1 异常，后续结果不可信 |"
    if [ "${NPU_SUITE_STOP_ON_FAIL}" = "1" ]; then
        _status "E0 失败且 STOP_ON_FAIL=1，中止。"
        bash "${COLLECT}" "${LOG_ROOT}" || true
        exit 1
    fi
else
    _status "| B E0 smoke | PASS |"
fi

_run_case "stable_pke_keygen" "${S_PKE_KG}" KEM_KEYGEN_FORCE_REBUILD=1 && _status "| B stable_pke_keygen | PASS |" || _status "| B stable_pke_keygen | FAIL |"
_run_case "stable_pke_encrypt" "${S_ENCRYPT}" KEM_KEYGEN_FORCE_REBUILD=1 && _status "| B stable_pke_encrypt | PASS |" || _status "| B stable_pke_encrypt | FAIL |"
_run_case "stable_pke_decrypt" "${S_DECRYPT}" KEM_KEYGEN_FORCE_REBUILD=1 && _status "| B stable_pke_decrypt | PASS |" || _status "| B stable_pke_decrypt | FAIL |"
_run_case "stable_kem_keygen_rebuild" "${S_KG}" KEM_KEYGEN_FORCE_REBUILD=1 && _status "| B stable_kem_keygen_rebuild | PASS |" || _status "| B stable_kem_keygen_rebuild | FAIL |"

# --- C: l18 诊断（必须在教材 #2 Encaps 之前）---
if _invoke_suite e1; then
    _status "| C E1 Encaps+TRACE | PASS |"
    if _invoke_suite e2; then
        _status "| C E2 Encaps repeat | PASS |"
    else
        _status "| C E2 Encaps repeat | FAIL |"
    fi
else
    L18_DIAG_FAIL=1
    _status "| C E1 Encaps+TRACE | **FAIL/HANG** — 见 npu_runs/*E1* 与 [l18-trace] |"
    _status "| C E2 | SKIP（E1 未过） |"
    if [ "${CONTINUE_L18}" != "1" ]; then
        _status "ONE_TRIP_CONTINUE_ON_L18_FAIL=0，中止。"
        bash "${COLLECT}" "${LOG_ROOT}" || true
        exit 1
    fi
    echo "[one_trip] E1 失败；继续 768/512/探针（不重复卡1 Encaps msprof）" >&2
fi

# --- D: 卡1 集成 + 分段 msprof（仅 E1 成功）---
if [ "${L18_DIAG_FAIL}" = "0" ]; then
    for phase in roundtrip msprof_decrypt msprof_decaps_e msprof_decaps; do
        if _invoke_suite "${phase}"; then
            _status "| D ${phase} | PASS |"
        else
            _status "| D ${phase} | FAIL |"
        fi
    done
else
    _status "| D roundtrip/msprof | SKIP（E1 未过，避免同卡污染） |"
fi

# --- E: 教材 14 档 msprof ---
if [ "${SKIP_TEXTBOOK}" != "1" ]; then
    if [ "${L18_DIAG_FAIL}" = "0" ]; then
        if _invoke_textbook "1,2,3,4"; then
            _status "| E textbook 1-4 | PASS |"
        else
            _status "| E textbook 1-4 | FAIL |"
        fi
    else
        if _invoke_textbook "1"; then
            _status "| E textbook #1 KeyGen only | PASS |"
        else
            _status "| E textbook #1 KeyGen only | FAIL |"
        fi
        _status "| E textbook 2-4 | SKIP（E1 未过） |"
    fi
    if _invoke_textbook "5,6,7,8,9,10,11,12"; then
        _status "| E textbook 5-12 | PASS |"
    else
        _status "| E textbook 5-12 | FAIL（768/512 含 l18，可能独立挂） |"
    fi
    if _invoke_textbook "13,14"; then
        _status "| E textbook 13-14 frozen | PASS |"
    else
        _status "| E textbook 13-14 frozen | FAIL |"
    fi
else
    _status "| E textbook | SKIP（ONE_TRIP_SKIP_TEXTBOOK=1） |"
fi

# --- F: 探针 ---
if [ "${SKIP_PROBES}" != "1" ]; then
    if _invoke_suite probes_safe; then
        _status "| F probes_safe | PASS |"
    else
        _status "| F probes_safe | FAIL |"
    fi
else
    _status "| F probes | SKIP |"
fi

# --- G: 可选 1024 incubating ---
if [ "${INCLUDE_1024_INC}" = "1" ]; then
    export TEXTBOOK_INCLUDE_1024_INCUBATING=1
    if [ "${L18_DIAG_FAIL}" = "0" ]; then
        if _invoke_suite examples; then
            _status "| G 1024 incubating | PASS |"
        else
            _status "| G 1024 incubating | FAIL |"
        fi
    else
        _status "| G 1024 incubating | SKIP Encaps/Decaps（E1 未过）；可手动 TEXTBOOK_ONLY=15 |"
        _invoke_textbook "15" || _status "| G exp keygen only | FAIL |"
    fi
fi

# --- H: 打包 ---
if [ "${DRY}" != "1" ]; then
    bash "${COLLECT}" "${LOG_ROOT}" || true
    _status ""
    _status "## 带回"
    _status ""
    _status "打包：\`${LOG_ROOT}/BRING_BACK.tar.gz\`"
    _status "软链：\`output/npu_one_trip/latest\`"
fi

ln -sfn "${LOG_ROOT}" "${REPO_ROOT}/output/npu_one_trip/latest" 2>/dev/null || true
echo "[one_trip] DONE log=${LOG_ROOT}/one_trip.log STATUS=${LOG_ROOT}/STATUS.md"
