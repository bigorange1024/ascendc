#!/usr/bin/env bash
# roundtrip_kem_decaps.sh — KEM 纯 device round-trip 第 3 步：读 sk + ct → ss 并与 Encaps ss 校验
#
# 作用：从同 RUN_MODE stash 取 dk 与 c，跑一次 Alg.21 Decaps，断言 Decaps.K == Encaps.K_enc。
# 前置：同模式已跑 keygen + encaps（encaps 须 stash m.bin）。
#
# Usage（CPU / SIM 分开跑）：
#   bash scripts/roundtrip_kem_decaps.sh -r cpu -v Ascend910B4
#   bash scripts/roundtrip_kem_decaps.sh -r sim -v Ascend910B4
#
# 环境（可选）：
#   SEED_D=20260619
#   ROUNDTRIP_KEM_STASH=<dir>   默认 output/roundtrip_kem/${RUN_MODE}
#
# 注意：SIM 默认 2-session（探针 run.sh）；Decaps 单段 ~11min，勿并行多 SIM。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DECAPS_DIR="${DECAPS_DIR:-${REPO_ROOT}/examples/stable/stable-fips203-mlkem-kem-decaps-ct-k4}"

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
    *) echo "[roundtrip_kem_decaps] unknown option $1" >&2; exit 1 ;;
    esac
done

if [ "${RUN_MODE}" != "cpu" ] && [ "${RUN_MODE}" != "sim" ]; then
    echo "[roundtrip_kem_decaps] ERROR: -r 须为 cpu 或 sim（一次只跑一种）" >&2
    exit 1
fi

STASH_DIR="${ROUNDTRIP_KEM_STASH:-${REPO_ROOT}/output/roundtrip_kem/${RUN_MODE}}"
DK_STASH="${STASH_DIR}/dk_kem.bin"
C_STASH="${STASH_DIR}/c.bin"
K_ENC_STASH="${STASH_DIR}/K_enc.bin"
M_STASH="${STASH_DIR}/m.bin"

for f in "${DK_STASH}" "${C_STASH}" "${K_ENC_STASH}" "${M_STASH}"; do
    if [ ! -f "${f}" ]; then
        echo "[roundtrip_kem_decaps] ERROR: 缺少 round-trip 中间态 ${f}" >&2
        echo "[roundtrip_kem_decaps] 请先按序跑 keygen → encaps（同 -r ${RUN_MODE}；encaps 须写出 m.bin）" >&2
        exit 2
    fi
done

echo "[roundtrip_kem_decaps] SEED_D=${SEED_D} RUN_MODE=${RUN_MODE} STASH=${STASH_DIR}"

# 关闭 run.sh 内 golden 对拍；闭环断言由 roundtrip_kem_verify.py 完成
# M_FILE：CPU twin Phase-E 读 golden_v（须与 Encaps 的 m 一致）
(cd "${DECAPS_DIR}" && SEED_D="${SEED_D}" \
    DK_KEM_SRC="${DK_STASH}" \
    C_SRC="${C_STASH}" \
    M_FILE="${M_STASH}" \
    K_ENC_SRC="${K_ENC_STASH}" \
    KEM_DECAPS_VERIFY=0 KEM_DECAPS_TAMPER_C=0 KEM_DECAPS_REJECT=0 \
    bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")

K_DEC_OUT="${DECAPS_DIR}/output/K.bin"
K_SZ=$(wc -c <"${K_DEC_OUT}")
if [ "${K_SZ}" -ne 32 ]; then
    echo "[roundtrip_kem_decaps] ERROR: K 尺寸 ${K_SZ} != 32" >&2
    exit 1
fi

cp -f "${K_DEC_OUT}" "${STASH_DIR}/K_dec.bin"

python3 "${REPO_ROOT}/scripts/roundtrip_kem_verify.py" agree \
    --k-enc "${K_ENC_STASH}" --k-dec "${STASH_DIR}/K_dec.bin"

echo "[SUCCESS] round-trip KEM decaps (${RUN_MODE}): Encaps.K == Decaps.K'"
