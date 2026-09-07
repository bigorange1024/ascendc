#!/bin/bash
# 实机整份串行：C0 → C1 → C2（禁止并行）
#
# Usage:
#   bash run_all_npu.sh -v Ascend910B4
#   TOY_ROUNDS=1 bash run_all_npu.sh   # 默认已是 1
#
# 看屏幕最后带 REPORT: 的行（例 REPORT: C1 PASS last=111）。
# 某档失败也会继续下一档，最后汇总。
set -uo pipefail

SUITE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOC_VERSION="Ascend910B4"

while [ $# -gt 0 ]; do
    case "$1" in
    -v | --soc-version)
        SOC_VERSION="$2"
        shift 2
        ;;
    *)
        echo "[ERROR] unknown arg: $1" >&2
        exit 1
        ;;
    esac
done

export TOY_ROUNDS="${TOY_ROUNDS:-1}"
export NPU_SKIP_GOLDEN="${NPU_SKIP_GOLDEN:-1}"
export TOY_SKIP_GOLDEN="${TOY_SKIP_GOLDEN:-1}"
export TOY_NPU_HOST_ONLY="${TOY_NPU_HOST_ONLY:-1}"

echo "============================================================"
echo "===== NPU_SUITE START SOC=${SOC_VERSION} rounds=${TOY_ROUNDS} ====="
echo "===== 只认 Host 三位码；设备 2xx/4xx 在 NPU 常看不见 ====="
echo "===== 请盯住带 REPORT: 的行打字回报 ====="
echo "============================================================"

_RESULTS=()

_run_case() {
    local tag="$1"
    local dir="$2"
    local rc=0
    echo ""
    echo "############################################################"
    echo "##### NPU_SUITE CASE ${tag} START #####"
    echo "############################################################"
    set +e
    bash "${dir}/run.sh" -r npu -v "${SOC_VERSION}"
    rc=$?
    set -e
    if [ "${rc}" -eq 0 ]; then
        echo "##### NPU_SUITE CASE ${tag} DONE rc=0 #####"
        _RESULTS+=("${tag}:OK")
    elif [ "${rc}" -eq 124 ]; then
        echo "##### NPU_SUITE CASE ${tag} TIMEOUT/HANG rc=124 #####"
        _RESULTS+=("${tag}:TIMEOUT124")
    else
        echo "##### NPU_SUITE CASE ${tag} FAIL rc=${rc} #####"
        _RESULTS+=("${tag}:FAIL${rc}")
    fi
    return 0
}

_run_case "C0" "${SUITE_DIR}/cases/C0-e01-handshake"
_run_case "C1" "${SUITE_DIR}/cases/C1-e13-glue"
_run_case "C2" "${SUITE_DIR}/cases/C2-e15-a2x2"

echo ""
echo "============================================================"
echo "===== NPU_SUITE SUMMARY（请把下面打字发回）====="
for r in "${_RESULTS[@]}"; do
    echo "SUMMARY ${r}"
done
echo "===== 也请把各档出现过的 REPORT: 行原样打字发回 ====="
echo "============================================================"

# 任一带 TIMEOUT 则非零；纯 verify 旧假红已修为 Host-only
_fail=0
for r in "${_RESULTS[@]}"; do
    case "$r" in
    *:TIMEOUT* | *:FAIL*) _fail=1 ;;
    esac
done
exit "${_fail}"
