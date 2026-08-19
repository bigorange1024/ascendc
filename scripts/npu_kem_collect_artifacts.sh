#!/usr/bin/env bash
# npu_kem_collect_artifacts.sh — 实机跑完后打包「一次搬码」可带回证据。
#
# 用法：
#   bash scripts/npu_kem_collect_artifacts.sh
#   bash scripts/npu_kem_collect_artifacts.sh output/npu_one_trip/<stamp>
#
# 产出：
#   <log_root>/BRING_BACK/          精简目录（日志摘要 + 指标 + csv 列表）
#   <log_root>/BRING_BACK.tar.gz
#   <log_root>/BRING_BACK/README.md  给 Agent / 协作者看的索引

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_ROOT="${1:-${REPO_ROOT}/output/npu_one_trip/latest}"
if [ ! -d "${LOG_ROOT}" ]; then
    echo "[collect] ERROR: 目录不存在 ${LOG_ROOT}" >&2
    exit 1
fi
LOG_ROOT="$(cd "${LOG_ROOT}" && pwd)"

OUT="${LOG_ROOT}/BRING_BACK"
rm -rf "${OUT}"
mkdir -p "${OUT}/logs" "${OUT}/snippets" "${OUT}/metrics" "${OUT}/msprof"

_copy_if() {
    local src="$1"
    local dst="$2"
    if [ -f "${src}" ]; then
        cp -f "${src}" "${dst}"
    fi
}

# 顶层日志
for f in one_trip.log STATUS.md suite.log; do
    _copy_if "${LOG_ROOT}/${f}" "${OUT}/logs/"
done
_copy_if "${LOG_ROOT}/summary.txt" "${OUT}/logs/"

# npu_run_case 日志树
if [ -d "${LOG_ROOT}/npu_runs" ]; then
    find "${LOG_ROOT}/npu_runs" -type f \( -name 'run.log' -o -name 'preflight.log' -o -name 'run_metrics.txt' \
        -o -name 'npu_launch_metrics.jsonl' -o -name 'msprof_csv_list.txt' \) -print0 2>/dev/null |
        while IFS= read -r -d '' f; do
            rel="${f#${LOG_ROOT}/npu_runs/}"
            mkdir -p "${OUT}/logs/npu_runs/$(dirname "${rel}")"
            cp -f "${f}" "${OUT}/logs/npu_runs/${rel}"
        done
fi

# 关键 stderr 片段：l18-trace / FAIL / timeout 124 / msprof_kernel
SNIP="${OUT}/snippets/key_lines.txt"
: >"${SNIP}"
grep -rhE '\[l18-trace\]|\[npu_launch\]|\[msprof_kernel|\[wall_sec\]|FAIL|exit 124|budget.*exceeded|max=' \
    "${LOG_ROOT}" "${REPO_ROOT}/output/npu_runs" 2>/dev/null >>"${SNIP}" || true
sort -u "${SNIP}" -o "${SNIP}" 2>/dev/null || true

# 各用例 output 指标（相对路径索引 + 复制小文件）
MANIFEST="${OUT}/case_outputs.txt"
: >"${MANIFEST}"
while IFS= read -r metrics; do
    [ -f "${metrics}" ] || continue
    case_dir="$(dirname "$(dirname "${metrics}")")"
    rel_case="${case_dir#${REPO_ROOT}/}"
    echo "${rel_case}" >>"${MANIFEST}"
    base="$(echo "${rel_case}" | tr '/' '_')"
    _copy_if "${metrics}" "${OUT}/metrics/${base}_run_metrics.txt"
    _copy_if "${case_dir}/output/npu_launch_metrics.jsonl" "${OUT}/metrics/${base}_npu_launch.jsonl"
done < <(find "${REPO_ROOT}/examples" "${REPO_ROOT}/ascendc-tests" -path '*/output/run_metrics.txt' 2>/dev/null | head -200)

# msprof csv 路径列表（不打包巨大 OPPROF 树，只列路径 + 复制 kernel_details）
CSV_LIST="${OUT}/msprof/kernel_details_paths.txt"
: >"${CSV_LIST}"
find "${REPO_ROOT}/examples" "${REPO_ROOT}/ascendc-tests" -path '*/prof_npu/*' -name 'kernel_details*.csv' 2>/dev/null |
    while read -r csv; do
        echo "${csv}" >>"${CSV_LIST}"
        base="$(echo "${csv#${REPO_ROOT}/}" | tr '/' '_')"
        cp -f "${csv}" "${OUT}/msprof/${base}" 2>/dev/null || true
    done

# 汇总 python（能跑则跑）
if [ -f "${REPO_ROOT}/scripts/npu_kem_textbook_list.txt" ]; then
    SUM="${OUT}/textbook_summaries.txt"
    : >"${SUM}"
    while IFS=$'\t' read -r id rel || [ -n "${id:-}" ]; do
        [[ -z "${id}" || "${id}" == \#* ]] && continue
        dir="${REPO_ROOT}/${rel}"
        if [ -d "${dir}" ]; then
            echo "=== #${id} ${rel} ===" >>"${SUM}"
            python3 "${REPO_ROOT}/scripts/npu_msprof_summarize.py" "${dir}" >>"${SUM}" 2>&1 || true
        fi
    done <"${REPO_ROOT}/scripts/npu_kem_textbook_list.txt"
fi

cat >"${OUT}/README.md" <<EOF
# 实机一次搬码 — 带回包

生成时间：$(date -Iseconds)
日志根：\`${LOG_ROOT}\`

## 优先看这些

1. \`logs/STATUS.md\` — 各块 PASS/FAIL 一览
2. \`snippets/key_lines.txt\` — \`[l18-trace]\` / \`[npu_launch]\` / FAIL / 124
3. \`textbook_summaries.txt\` — 教材档 msprof 求和（若有）
4. \`msprof/kernel_details_paths.txt\` — 实机 csv 原路径；小 csv 在 \`msprof/\`

## l18 诊断

E1 日志在 \`logs/npu_runs/*E1*\`。把 **全部** \`[l18-trace]\` 行贴回 qa/ 或 Agent。

## 填表

设备真值：\`[msprof_kernel_total]\`（kernel_details 求和）；host 对照：\`[npu_launch_total]\`。

EOF

(
    cd "${LOG_ROOT}"
    tar -czf BRING_BACK.tar.gz -C "${LOG_ROOT}" BRING_BACK
)
echo "[collect] OK ${LOG_ROOT}/BRING_BACK.tar.gz ($(du -h "${LOG_ROOT}/BRING_BACK.tar.gz" | awk '{print $1}'))"
