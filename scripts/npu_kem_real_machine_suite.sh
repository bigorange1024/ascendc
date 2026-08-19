#!/usr/bin/env bash
# npu_kem_real_machine_suite.sh — 1024 ML-KEM 借入实机 **一次性搬码** 验收套件（stable 为主，探针可选）。
#
# 设计目标（用户 2026-08-18）：
#   - 不在实机上「改一行测一行」；本脚本 + npu_run_case.sh + npu_card_guard.sh 事先写好。
#   - 默认只跑 **低风险 stable**；Encaps/Decaps/l18 风险项须显式 PHASE 或 SKIP_L18_RISK=0。
#   - 无 liboqs 可跑（gen_data 走 kem_ref python 回落）；不做 liboqs 交叉。
#   - 卡死/timeout 124 → 打印污染恢复步骤，停止后续 l18 相关 phase。
#
# 用法（在仓库根，先 source CANN 或由 run.sh 内 source env.sh）：
#   bash scripts/npu_kem_real_machine_suite.sh                    # 默认 PHASE=smoke
#   NPU_SUITE_PHASE=e0 bash scripts/npu_kem_real_machine_suite.sh
#   NPU_SUITE_PHASE=msprof_decaps bash scripts/npu_kem_real_machine_suite.sh
#   NPU_SUITE_PHASE=all SKIP_L18_RISK=0 bash scripts/npu_kem_real_machine_suite.sh  # 高风险
#
# PHASE（NPU_SUITE_PHASE）：
#   smoke          — PKE Encrypt + KEM KeyGen stable（默认；无 l18 全链）
#   stable         — 1024 stable 七算子按序（先无 l18，Encaps/Decaps 受 SKIP_L18_RISK）
#   examples       — 1024 incubating（卡 2；Encaps/Decaps 受 SKIP_L18_RISK）
#   e0             — 同 smoke（对齐 qa 08-05 E0）
#   e1             — Encaps stable + F203_L18_TRACE=1（仅干净卡；失败则停）
#   e2             — Encaps stable 再跑一遍（测污染；须 e1 已成功）
#   msprof_decrypt — stable PKE Decrypt msprof（Decaps Phase-D 同源核）
#   msprof_decaps_e— stable Decaps KEM_DECAPS_PHASEE_ONLY=1 msprof（E 段 2 launch）
#   msprof_decaps  — stable Decaps 全链 msprof（3 launch；l18 风险）
#   roundtrip      — device KeyGen→Encaps→Decaps（stable 路径，SEED_D 定点）
#   probes_safe    — alg13/14/15/19 探针 npu 冒烟（无 alg20/21）
#   textbook       — 教材 14 档 KEM 性能+profiling（转 npu_kem_textbook_perf.sh）
#   one_trip       — 实机一次搬码测全（转 npu_kem_one_trip.sh；见 docs/engineering/实机一次搬码验收清单.md）
#   all            — 顺序执行 smoke → msprof_decrypt → msprof_decaps_e → roundtrip
#                    （不含 e1/e2/msprof_decaps 除非 SKIP_L18_RISK=0）
#
# 环境：
#   ASCEND_DEVICE_ID            显式则全 suite 用该卡；否则按 npu_device_map（stable=1/examples=2/tests=3）
#   SOC_VERSION=Ascend910B4
#   SKIP_L18_RISK=1             默认跳过 Encaps/Decaps 全链 / e1 / e2 / msprof_decaps
#   NPU_SUITE_FORCE_REBUILD=1   各用例 FORCE_REBUILD（默认 1，实机 pull 后建议）
#   NPU_SUITE_DRY_RUN=1         只打印分卡/将跑的用例，不编译不跑（搬码前自检）
#   NPU_SUITE_STOP_ON_FAIL=1    某 phase 失败则停止（默认 1）
#
# 日志根：output/npu_suite/<stamp>/suite.log

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_CASE="${REPO_ROOT}/scripts/npu_run_case.sh"
PHASE="${NPU_SUITE_PHASE:-smoke}"
STAMP="$(date +%Y%m%d_%H%M%S)"
SUITE_LOG_ROOT="${REPO_ROOT}/output/npu_suite/${STAMP}"
mkdir -p "${SUITE_LOG_ROOT}"

export SOC_VERSION="${SOC_VERSION:-Ascend910B4}"
# 不在此写死 ASCEND_DEVICE_ID=0：各用例由 npu_device_map / npu_run_case 按树分卡。
# 若调用方已 export ASCEND_DEVICE_ID，则全 suite 沿用（显式覆盖）。
SKIP_L18_RISK="${SKIP_L18_RISK:-1}"
STOP_ON_FAIL="${NPU_SUITE_STOP_ON_FAIL:-1}"
FORCE_RB="${NPU_SUITE_FORCE_REBUILD:-1}"

STABLE="${REPO_ROOT}/examples/stable/ml-kem/ml-kem-1024"
PROBE="${REPO_ROOT}/ascendc-tests/ml-kem/ml-kem-1024"

S_ENCRYPT="${STABLE}/stable-fips203-mlkem-pke-encrypt-k4"
S_DECRYPT="${STABLE}/stable-fips203-mlkem-pke-decrypt-k4"
S_PKE_KG="${STABLE}/stable-fips203-mlkem-pke-keygen-k4"
S_KG="${STABLE}/stable-fips203-mlkem-kem-keygen-k4"
S_ENCAPS="${STABLE}/stable-fips203-mlkem-kem-encaps-k4"
S_DECAPS="${STABLE}/stable-fips203-mlkem-kem-decaps-k4"
S_DECAPS_CT="${STABLE}/stable-fips203-mlkem-kem-decaps-ct-k4"

INCUB="${REPO_ROOT}/examples/incubating/ml-kem/ml-kem-1024"
E_PKE_KG="${INCUB}/exp-fips203-mlkem-pke-keygen-k4"
E_ENCRYPT="${INCUB}/exp-fips203-mlkem-pke-encrypt-k4"
E_DECRYPT="${INCUB}/exp-fips203-mlkem-pke-decrypt-k4"
E_KG="${INCUB}/exp-fips203-mlkem-kem-keygen-k4"
E_ENCAPS="${INCUB}/exp-fips203-mlkem-kem-encaps-k4"
E_DECAPS="${INCUB}/exp-fips203-mlkem-kem-decaps-k4"

P_KG="${PROBE}/pass-fix-f203-alg13-device-keygen-k4"
P_ENC="${PROBE}/pass-fix-f203-alg14-pke-encrypt-device-k4"
P_DEC="${PROBE}/pass-fix-f203-alg15-pke-decrypt-device-k4"
P_KEM_KG="${PROBE}/pass-fix-f203-alg19-kem-keygen-device-k4"

echo "[npu_suite] log=${SUITE_LOG_ROOT}/suite.log PHASE=${PHASE} DRY_RUN=${NPU_SUITE_DRY_RUN:-0}"
if [ "${NPU_SUITE_DRY_RUN:-0}" != "1" ]; then
    exec >>"${SUITE_LOG_ROOT}/suite.log" 2>&1
fi
echo "[npu_suite] start stamp=${STAMP} PHASE=${PHASE} device=${ASCEND_DEVICE_ID:-<per-case map>} SKIP_L18_RISK=${SKIP_L18_RISK}"

_run() {
    local label="$1"
    local dir="$2"
    shift 2
    local msprof=0
    local env_args=()
    while [ "$#" -gt 0 ]; do
        case "$1" in
        --msprof) msprof=1; shift ;;
        *=*) env_args+=("$1"); shift ;;
        *) echo "[npu_suite] WARN: 忽略未知参数 $1"; shift ;;
        esac
    done
    local rb_flag=()
    if [ "${FORCE_RB}" = "1" ]; then
        rb_flag=(--force-rebuild)
    fi
    local dry_flag=()
    if [ "${NPU_SUITE_DRY_RUN:-0}" = "1" ]; then
        dry_flag=(--dry-run)
    fi
    local case_args=(--dir "${dir}" --label "${label}")
    if [ "${msprof}" = "1" ]; then
        case_args+=(--msprof)
    fi
    echo "[npu_suite] === ${label} env=[${env_args[*]}] ==="
    if ! env "${env_args[@]}" bash "${RUN_CASE}" "${case_args[@]}" "${rb_flag[@]}" "${dry_flag[@]}"; then
        echo "[npu_suite] FAIL phase=${label}" >&2
        if [ "${STOP_ON_FAIL}" = "1" ]; then
            echo "[npu_suite] STOP_ON_FAIL=1，中止后续 phase" >&2
            exit 1
        fi
        return 1
    fi
    return 0
}

_phase_smoke() {
    _run E0_stable_encrypt "${S_ENCRYPT}"
    _run E0_stable_kem_keygen "${S_KG}"
}

# 用户实机顺序：先全部 stable（卡 1），再 examples（卡 2），再 probes（卡 3）
_phase_stable() {
    _run stable_pke_keygen "${S_PKE_KG}"
    _run stable_pke_encrypt "${S_ENCRYPT}"
    _run stable_pke_decrypt "${S_DECRYPT}"
    _run stable_kem_keygen "${S_KG}"
    if [ "${SKIP_L18_RISK}" = "1" ]; then
        echo "[npu_suite] SKIP stable Encaps/Decaps（l18）；SKIP_L18_RISK=0 才跑"
        return 0
    fi
    _run stable_kem_encaps "${S_ENCAPS}"
    _run stable_kem_decaps "${S_DECAPS}"
    _run stable_kem_decaps_ct "${S_DECAPS_CT}"
}

_phase_examples() {
    _run exp_pke_keygen "${E_PKE_KG}"
    _run exp_pke_encrypt "${E_ENCRYPT}"
    _run exp_pke_decrypt "${E_DECRYPT}"
    _run exp_kem_keygen "${E_KG}"
    if [ "${SKIP_L18_RISK}" = "1" ]; then
        echo "[npu_suite] SKIP incubating Encaps/Decaps（l18）；SKIP_L18_RISK=0 才跑"
        return 0
    fi
    _run exp_kem_encaps "${E_ENCAPS}"
    _run exp_kem_decaps "${E_DECAPS}"
}

_phase_e0() {
    _phase_smoke
}

_phase_e1() {
    if [ "${SKIP_L18_RISK}" = "1" ]; then
        echo "[npu_suite] SKIP e1（Encaps+l18）；设 SKIP_L18_RISK=0 才跑"
        return 0
    fi
    _run E1_encaps_trace "${S_ENCAPS}" F203_L18_TRACE=1
}

_phase_e2() {
    if [ "${SKIP_L18_RISK}" = "1" ]; then
        echo "[npu_suite] SKIP e2"
        return 0
    fi
    _run E2_encaps_repeat "${S_ENCAPS}"
}

_phase_msprof_decrypt() {
    _run msprof_stable_decrypt "${S_DECRYPT}" --msprof
}

_phase_msprof_decaps_e() {
    _run msprof_decaps_phase_e "${S_DECAPS}" KEM_DECAPS_PHASEE_ONLY=1 --msprof
}

_phase_msprof_decaps() {
    if [ "${SKIP_L18_RISK}" = "1" ]; then
        echo "[npu_suite] SKIP msprof_decaps 全链（l18）；Decaps-D 用 msprof_decrypt，E 用 msprof_decaps_e"
        return 0
    fi
    _run msprof_decaps_full "${S_DECAPS}" MSPROF_LAUNCH_COUNT_NPU=32 --msprof
}

_phase_roundtrip() {
    echo "[npu_suite] === roundtrip KeyGen→Encaps→Decaps（stable 三件套，SEED_D 定点）==="
    if [ "${NPU_SUITE_DRY_RUN:-0}" = "1" ]; then
        echo "[npu_suite] DRY_RUN skip roundtrip_kem script; devices would follow npu_device_map per dir"
        echo "[npu_suite]   KEYGEN=${KEYGEN_DIR:-${S_KG}}"
        echo "[npu_suite]   ENCAPS=${S_ENCAPS}"
        echo "[npu_suite]   DECAPS=${S_DECAPS}"
        return 0
    fi
    export SEED_D="${SEED_D:-20260619}"
    export KEYGEN_DIR="${STABLE}/stable-fips203-mlkem-kem-keygen-k4"
    export ENCAPS_DIR="${S_ENCAPS}"
    export DECAPS_DIR="${S_DECAPS}"
    export NPU_RUN_LOG_ROOT="${SUITE_LOG_ROOT}/roundtrip"
    mkdir -p "${NPU_RUN_LOG_ROOT}"
    if ! bash "${REPO_ROOT}/scripts/roundtrip_kem_keygen_encaps_decaps.sh" -r npu -v "${SOC_VERSION}"; then
        echo "[npu_suite] FAIL roundtrip" >&2
        # shellcheck source=/dev/null
        source "${REPO_ROOT}/scripts/npu_card_guard.sh"
        npu_card_guard_on_failure 1 "roundtrip_kem"
        [ "${STOP_ON_FAIL}" = "1" ] && exit 1
    fi
}

_phase_probes_safe() {
    _run probe_alg13_keygen "${P_KG}"
    _run probe_alg14_encrypt "${P_ENC}"
    _run probe_alg15_decrypt "${P_DEC}"
    _run probe_alg19_kem_keygen "${P_KEM_KG}"
}

_phase_all() {
    _phase_stable || exit 1
    _phase_msprof_decrypt || exit 1
    if [ "${SKIP_L18_RISK}" = "0" ]; then
        _phase_msprof_decaps_e || exit 1
        _phase_roundtrip || exit 1
        _phase_e1 || exit 1
        _phase_e2 || exit 1
        _phase_msprof_decaps || exit 1
    else
        echo "[npu_suite] all：SKIP_L18_RISK=1 已跳过 Encaps/Decaps/msprof E 段/roundtrip"
    fi
}

case "${PHASE}" in
smoke) _phase_smoke ;;
stable) _phase_stable ;;
examples) _phase_examples ;;
e0) _phase_e0 ;;
e1) _phase_e1 ;;
e2) _phase_e2 ;;
msprof_decrypt) _phase_msprof_decrypt ;;
msprof_decaps_e) _phase_msprof_decaps_e ;;
msprof_decaps) _phase_msprof_decaps ;;
roundtrip) _phase_roundtrip ;;
probes_safe) _phase_probes_safe ;;
textbook)
    echo "[npu_suite] PHASE=textbook → scripts/npu_kem_textbook_perf.sh"
    bash "${REPO_ROOT}/scripts/npu_kem_textbook_perf.sh"
    ;;
one_trip)
    echo "[npu_suite] PHASE=one_trip → scripts/npu_kem_one_trip.sh"
    bash "${REPO_ROOT}/scripts/npu_kem_one_trip.sh"
    ;;
all) _phase_all ;;
*)
    echo "[npu_suite] unknown PHASE=${PHASE}" >&2
    exit 1
    ;;
esac

echo "[npu_suite] DONE PHASE=${PHASE} log=${SUITE_LOG_ROOT}/suite.log"
ln -sfn "${SUITE_LOG_ROOT}" "${REPO_ROOT}/output/npu_suite/latest" 2>/dev/null || true
