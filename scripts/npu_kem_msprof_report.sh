#!/usr/bin/env bash
# npu_kem_msprof_report.sh — 汇总实机 msprof 产物：csv 列表 + kernel_details 行数 + 关键字 kernel 名。
#
# 用法：
#   bash scripts/npu_kem_msprof_report.sh examples/stable/.../stable-fips203-mlkem-kem-decaps-k4
#   bash scripts/npu_kem_msprof_report.sh --suite output/npu_suite/latest

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET=""

while [ "$#" -gt 0 ]; do
    case "$1" in
    --suite)
        TARGET="$(readlink -f "$2" 2>/dev/null || echo "$2")"
        shift 2
        ;;
    *)
        TARGET="$(cd "$1" && pwd)"
        shift
        ;;
    esac
done

if [ -z "${TARGET}" ]; then
    echo "usage: $0 <case_dir|prof_npu_dir> | --suite output/npu_suite/latest" >&2
    exit 1
fi

_prof_roots=()
if [ -d "${TARGET}/prof_npu" ]; then
    _prof_roots+=("${TARGET}/prof_npu")
elif [ -f "${TARGET}/suite.log" ]; then
    while IFS= read -r d; do
        [ -d "${d}/prof_npu" ] && _prof_roots+=("${d}/prof_npu")
    done < <(grep -oE 'dir=[^ ]+' "${TARGET}/suite.log" 2>/dev/null | sed 's/dir=//' | sort -u || true)
fi
if [ "${#_prof_roots[@]}" -eq 0 ] && [ -d "${TARGET}" ]; then
    _prof_roots+=("${TARGET}")
fi

echo "[msprof_report] target=${TARGET}"
for root in "${_prof_roots[@]}"; do
    echo "[msprof_report] === prof root: ${root} ==="
    find "${root}" -name '*.csv' 2>/dev/null | sort | while read -r f; do
        echo "  csv: ${f} ($(wc -l <"${f}") lines)"
    done
    find "${root}" -name 'kernel_details*.csv' 2>/dev/null | while read -r kf; do
        echo "--- ${kf} ---"
        head -3 "${kf}"
        grep -iE 'decrypt|phase_e|l18_l19|Kernel|Op Name' "${kf}" 2>/dev/null | head -15 || true
        echo "(total data lines: $(($(wc -l <"${kf}") - 1)))"
    done
done

echo "[msprof_report] 说明：NPU 无 CAModel Total tick。逐 kernel 以 kernel_details 求和为准（见 npu_msprof_summarize.py）；勿信终端单行 task duration。"
echo "[msprof_report] Decaps 全链 3 launch 应对应 kernel_details 多行。仅 1 行 decrypt → 可能只跑到 Phase-D 或 hang 在 E/l18。"

if command -v python3 >/dev/null 2>&1; then
    python3 "${REPO_ROOT}/scripts/npu_msprof_summarize.py" "${TARGET}" || true
fi
