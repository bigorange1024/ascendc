#!/usr/bin/env bash
# roundtrip_kem_keygen_encaps_decaps.sh — device KeyGen→Encaps→Decaps 纯设备闭环（不借 liboqs）
#
# 分项脚本（推荐，CPU/SIM 分开、各跑一次）：
#   bash scripts/roundtrip_kem_keygen.sh  -r cpu|sim -v Ascend910B4
#   bash scripts/roundtrip_kem_encaps.sh  -r cpu|sim -v Ascend910B4
#   bash scripts/roundtrip_kem_decaps.sh  -r cpu|sim -v Ascend910B4
# stash：output/roundtrip_kem/<cpu|sim>/
#
# 验证：Decaps(dk, Encaps(ek).c).K == Encaps.K。
# 拒绝路径：KEM_DECAPS_REJECT=1（Gate E3 随机假密文）→ K == J(z‖c_bad) 且 ≠ Encaps.K。
#
# Usage：
#   bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r sim -v Ascend910B4
#   bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r npu -v Ascend910B4   # 实机；跑前 npu_card_guard preflight
#   推荐经套件：NPU_SUITE_PHASE=roundtrip bash scripts/npu_kem_real_machine_suite.sh
#
# 环境（可选）：
#   SEED_D                       默认 20260619
#   ROUNDTRIP_KEM_SKIP_REJECT=1  跳过拒绝路径
#
# 注意：SIM 下 Decaps 2-session，合法+拒绝各约十余分钟；勿并行多 SIM。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KEYGEN_DIR="${KEYGEN_DIR:-${REPO_ROOT}/ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4}"
ENCAPS_DIR="${ENCAPS_DIR:-${REPO_ROOT}/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4}"
DECAPS_DIR="${DECAPS_DIR:-${REPO_ROOT}/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4}"

RUN_MODE="cpu"
SOC_VERSION="Ascend910B4"
export SEED_D="${SEED_D:-20260619}"

SHORT=r:,v:
LONG=run-mode:,soc-version:
OPTS=$(getopt -a --options "$SHORT" --longoptions "$LONG" -- "$@")
eval set -- "$OPTS"
while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[roundtrip_kem] unknown option $1" >&2; exit 1 ;;
    esac
done

STASH_DIR="${ROUNDTRIP_KEM_STASH:-${REPO_ROOT}/output/roundtrip_kem/${RUN_MODE}}"
mkdir -p "${STASH_DIR}"
echo "[roundtrip_kem] SEED_D=${SEED_D} RUN_MODE=${RUN_MODE} STASH=${STASH_DIR}"

# 实机 npu：跑前确认相关卡干净；**不** export 全局 ASCEND_DEVICE_ID，
# 以便 KeyGen 探针（卡 3）与 stable Encaps/Decaps（卡 1）各走 npu_device_map。
if [ "${RUN_MODE}" = "npu" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/npu_device_map.sh"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/npu_card_guard.sh"
    _rt_ids=()
    if [ -n "${ASCEND_DEVICE_ID+x}" ]; then
        _rt_ids+=("${ASCEND_DEVICE_ID}")
    else
        _rt_ids+=("$(npu_device_id_for_path "${KEYGEN_DIR}")")
        _rt_ids+=("$(npu_device_id_for_path "${ENCAPS_DIR}")")
        _rt_ids+=("$(npu_device_id_for_path "${DECAPS_DIR}")")
    fi
    _seen=" "
    for _id in "${_rt_ids[@]}"; do
        case "${_seen}" in
        *" ${_id} "*) continue ;;
        esac
        _seen="${_seen}${_id} "
        echo "[roundtrip_kem] preflight device=${_id}"
        if ! ASCEND_DEVICE_ID="${_id}" npu_card_guard_preflight; then
            echo "[roundtrip_kem] npu preflight 未通过 device=${_id}" >&2
            exit 2
        fi
    done
    unset _rt_ids _seen _id
fi

_roundtrip_fail() {
    local rc="$1"
    local phase="$2"
    if [ "${RUN_MODE}" = "npu" ]; then
        # shellcheck source=/dev/null
        source "${REPO_ROOT}/scripts/npu_card_guard.sh" 2>/dev/null || true
        npu_card_guard_on_failure "${rc}" "roundtrip_${phase}" >&2
    fi
    exit "${rc}"
}

echo "[roundtrip_kem] === Phase 1: device KeyGen ==="
if ! (cd "${KEYGEN_DIR}" && SEED_D="${SEED_D}" KEM_KEYGEN_VERIFY=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}"); then
    _roundtrip_fail "$?" "keygen"
fi
cp -f "${KEYGEN_DIR}/output/dk_kem.bin" "${STASH_DIR}/dk_kem.bin"
cp -f "${KEYGEN_DIR}/output/ek_kem.bin" "${STASH_DIR}/ek_kem.bin"

echo "[roundtrip_kem] === Phase 2: device Encaps ==="
if ! (cd "${ENCAPS_DIR}" && SEED_D="${SEED_D}" \
    EK_KEM_SRC="${STASH_DIR}/ek_kem.bin" \
    KEM_ENCAPS_VERIFY=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}"); then
    _roundtrip_fail "$?" "encaps"
fi
cp -f "${ENCAPS_DIR}/output/c.bin" "${STASH_DIR}/c.bin"
cp -f "${ENCAPS_DIR}/output/K.bin" "${STASH_DIR}/K_enc.bin"
cp -f "${ENCAPS_DIR}/input/m.bin" "${STASH_DIR}/m.bin"

echo "[roundtrip_kem] === Phase 3: device Decaps (accept) ==="
if ! (cd "${DECAPS_DIR}" && SEED_D="${SEED_D}" \
    DK_KEM_SRC="${STASH_DIR}/dk_kem.bin" \
    C_SRC="${STASH_DIR}/c.bin" \
    M_FILE="${STASH_DIR}/m.bin" \
    K_ENC_SRC="${STASH_DIR}/K_enc.bin" \
    KEM_DECAPS_VERIFY=0 KEM_DECAPS_TAMPER_C=0 KEM_DECAPS_REJECT=0 \
    bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}"); then
    _roundtrip_fail "$?" "decaps_accept"
fi
cp -f "${DECAPS_DIR}/output/K.bin" "${STASH_DIR}/K_dec.bin"

python3 "${REPO_ROOT}/scripts/roundtrip_kem_verify.py" agree \
    --k-enc "${STASH_DIR}/K_enc.bin" --k-dec "${STASH_DIR}/K_dec.bin"

if [ "${ROUNDTRIP_KEM_SKIP_REJECT:-0}" != "1" ]; then
    echo "[roundtrip_kem] === Phase 4: device Decaps (reject via KEM_DECAPS_REJECT / E3) ==="
    if ! (cd "${DECAPS_DIR}" && SEED_D="${SEED_D}" \
        DK_KEM_SRC="${STASH_DIR}/dk_kem.bin" \
        KEM_DECAPS_VERIFY=0 KEM_DECAPS_TAMPER_C=0 KEM_DECAPS_REJECT=1 \
        bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}"); then
        _roundtrip_fail "$?" "decaps_reject"
    fi
    cp -f "${DECAPS_DIR}/output/K.bin" "${STASH_DIR}/K_reject.bin"
    cp -f "${DECAPS_DIR}/input/c.bin" "${STASH_DIR}/c_reject.bin"
    python3 "${REPO_ROOT}/scripts/roundtrip_kem_verify.py" reject \
        --dk "${STASH_DIR}/dk_kem.bin" \
        --c "${STASH_DIR}/c_reject.bin" \
        --k "${STASH_DIR}/K_reject.bin" \
        --k-enc "${STASH_DIR}/K_enc.bin"
else
    echo "[roundtrip_kem] Phase 4 拒绝路径已跳过（ROUNDTRIP_KEM_SKIP_REJECT=1）"
fi

echo "[SUCCESS] round-trip KEM KeyGen→Encaps→Decaps(+reject) (${RUN_MODE}) SEED_D=${SEED_D}"
