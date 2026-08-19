#!/usr/bin/env bash
# npu_run_case.sh — 在用例目录外包一层：实机 preflight、统一日志、失败善后、可选 msprof。
#
# 用法：
#   bash scripts/npu_run_case.sh --dir examples/stable/.../stable-fips203-mlkem-pke-encrypt-k4
#   bash scripts/npu_run_case.sh --dir ... --msprof --force-rebuild --label E0_encrypt
#   RUN_WITH_MSPROF=1 也可由 --msprof 注入
#
# 选项：
#   --dir PATH          用例根（须含 run.sh）
#   --label NAME        日志前缀（默认目录名）
#   --msprof            等价 RUN_WITH_MSPROF=1
#   --force-rebuild     注入 KEM_* / KEYGEN_* / ENCRYPT_* FORCE_REBUILD=1（按目录名启发）
#   --skip-preflight    跳过 npu_card_guard_preflight
#   --dry-run           只打印分卡结果，不编译不跑（NPU_RUN_DRY_RUN=1 等价）
#   -- SOC / device 通过环境：SOC_VERSION；ASCEND_DEVICE_ID 显式优先，否则 npu_device_map
#
# 日志：${NPU_RUN_LOG_ROOT:-output/npu_runs}/<stamp>_<label>/run.log
# 产物：用例内 output/、prof_npu/ 仍在用例目录（不搬）。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CASE_DIR=""
LABEL=""
MSPROF=0
SKIP_PREFLIGHT=0
FORCE_REBUILD=0
DRY_RUN=0

while [ "$#" -gt 0 ]; do
    case "$1" in
    --dir) CASE_DIR="$2"; shift 2 ;;
    --label) LABEL="$2"; shift 2 ;;
    --msprof) MSPROF=1; shift ;;
    --force-rebuild) FORCE_REBUILD=1; shift ;;
    --skip-preflight) SKIP_PREFLIGHT=1; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    -h | --help)
        sed -n '1,35p' "$0"
        exit 0
        ;;
    *)
        echo "[npu_run_case] unknown arg: $1" >&2
        exit 1
        ;;
    esac
done

if [ -z "${CASE_DIR}" ] || [ ! -f "${CASE_DIR}/run.sh" ]; then
    echo "[npu_run_case] ERROR: --dir 须指向含 run.sh 的用例目录" >&2
    exit 1
fi
CASE_DIR="$(cd "${CASE_DIR}" && pwd)"
if [ -z "${LABEL}" ]; then
    LABEL="$(basename "${CASE_DIR}")"
fi

STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_ROOT="${NPU_RUN_LOG_ROOT:-${REPO_ROOT}/output/npu_runs}"
LOG_DIR="${LOG_ROOT}/${STAMP}_${LABEL}"
mkdir -p "${LOG_DIR}"

export SOC_VERSION="${SOC_VERSION:-Ascend910B4}"
# 未显式指定时按用例树分卡（stable=1 / examples=2 / tests=3）；勿在此写死 0
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/npu_device_map.sh"
npu_device_map_apply "${CASE_DIR}"
if [ "${DRY_RUN}" = "1" ] || [ "${NPU_RUN_DRY_RUN:-0}" = "1" ] || [ "${NPU_SUITE_DRY_RUN:-0}" = "1" ]; then
    echo "[npu_run_case] DRY_RUN label=${LABEL} dir=${CASE_DIR} device=${ASCEND_DEVICE_ID}"
    exit 0
fi
if [ "${MSPROF}" = "1" ]; then
    export RUN_WITH_MSPROF=1
    export MSPROF_MODE="${MSPROF_MODE:-app}"
    export MSPROF_LAUNCH_COUNT_NPU="${MSPROF_LAUNCH_COUNT_NPU:-64}"
fi

# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/npu_card_guard.sh"

if [ "${SKIP_PREFLIGHT}" != "1" ]; then
    if ! npu_card_guard_preflight >>"${LOG_DIR}/preflight.log" 2>&1; then
        cat "${LOG_DIR}/preflight.log"
        echo "[npu_run_case] preflight 未通过；见 ${LOG_DIR}/preflight.log" >&2
        echo "[npu_run_case] 若确认卡干净：NPU_GUARD_STRICT=0 bash scripts/npu_run_case.sh …" >&2
        exit 2
    fi
fi

# 按目录名注入 FORCE_REBUILD（实机首跑 / pull 后建议开）
if [ "${FORCE_REBUILD}" = "1" ]; then
    case "${LABEL}" in
    *keygen*) export KEM_KEYGEN_FORCE_REBUILD=1 ;;
    *encaps*) export KEM_ENCAPS_FORCE_REBUILD=1 ;;
    *decaps*) export KEM_DECAPS_FORCE_REBUILD=1 ;;
    *encrypt*) export ENCRYPT_FORCE_REBUILD=1 ;;
    *decrypt*) export DECRYPT_FORCE_REBUILD=1 ;;
    esac
    export KEYGEN_FORCE_REBUILD=1
fi

echo "[npu_run_case] label=${LABEL} dir=${CASE_DIR} device=${ASCEND_DEVICE_ID} msprof=${RUN_WITH_MSPROF:-0}"
echo "[npu_run_case] log=${LOG_DIR}/run.log"

set +e
(
    cd "${CASE_DIR}"
    bash run.sh -r npu -v "${SOC_VERSION}"
) 2>&1 | tee "${LOG_DIR}/run.log"
rc=${PIPESTATUS[0]}
set -e

# 软链方便找最近日志
ln -sfn "${LOG_DIR}" "${LOG_ROOT}/latest_${LABEL}" 2>/dev/null || true

# msprof 摘要写入 log_dir
if [ "${RUN_WITH_MSPROF:-0}" = "1" ]; then
    find "${CASE_DIR}/prof_npu" -name '*.csv' 2>/dev/null >"${LOG_DIR}/msprof_csv_list.txt" || true
    if [ -f "${CASE_DIR}/output/run_metrics.txt" ]; then
        cp -f "${CASE_DIR}/output/run_metrics.txt" "${LOG_DIR}/run_metrics.txt" 2>/dev/null || true
    fi
    if [ -f "${CASE_DIR}/output/npu_launch_metrics.jsonl" ]; then
        cp -f "${CASE_DIR}/output/npu_launch_metrics.jsonl" "${LOG_DIR}/npu_launch_metrics.jsonl" 2>/dev/null || true
    fi
fi

if [ "${rc}" -eq 0 ]; then
    npu_card_guard_post_success "${LABEL}" | tee -a "${LOG_DIR}/run.log"
    echo "[npu_run_case] PASS ${LABEL} (log=${LOG_DIR})"
else
    npu_card_guard_on_failure "${rc}" "${LABEL}" | tee -a "${LOG_DIR}/run.log"
    echo "[npu_run_case] FAIL ${LABEL} rc=${rc} (log=${LOG_DIR})" >&2
fi
exit "${rc}"
