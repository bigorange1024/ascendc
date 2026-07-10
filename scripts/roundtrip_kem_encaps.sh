#!/usr/bin/env bash
# roundtrip_kem_encaps.sh — KEM 纯 device round-trip 第 2 步：读 pk → 产出 ct + ss
#
# 作用：从同 RUN_MODE stash 取 ek，跑一次 Alg.20 Encaps，写入 c.bin 与 K_enc.bin（共享秘密）。
# 前置：同模式已跑 roundtrip_kem_keygen.sh
#
# Usage（CPU / SIM 分开跑）：
#   bash scripts/roundtrip_kem_encaps.sh -r cpu -v Ascend910B4
#   bash scripts/roundtrip_kem_encaps.sh -r sim -v Ascend910B4
#
# 环境（可选）：
#   SEED_D=20260619
#   ROUNDTRIP_KEM_STASH=<dir>   默认 output/roundtrip_kem/${RUN_MODE}

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENCAPS_DIR="${ENCAPS_DIR:-${REPO_ROOT}/ascendc-tests/fix-f203-alg20-kem-encaps-correctness-k4}"

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
    *) echo "[roundtrip_kem_encaps] unknown option $1" >&2; exit 1 ;;
    esac
done

if [ "${RUN_MODE}" != "cpu" ] && [ "${RUN_MODE}" != "sim" ]; then
    echo "[roundtrip_kem_encaps] ERROR: -r 须为 cpu 或 sim（一次只跑一种）" >&2
    exit 1
fi

STASH_DIR="${ROUNDTRIP_KEM_STASH:-${REPO_ROOT}/output/roundtrip_kem/${RUN_MODE}}"
EK_STASH="${STASH_DIR}/ek_kem.bin"

if [ ! -f "${EK_STASH}" ]; then
    echo "[roundtrip_kem_encaps] ERROR: 缺少 pk，请先跑同模式 keygen：" >&2
    echo "  ${EK_STASH}" >&2
    echo "  bash scripts/roundtrip_kem_keygen.sh -r ${RUN_MODE} -v ${SOC_VERSION}" >&2
    exit 2
fi

echo "[roundtrip_kem_encaps] SEED_D=${SEED_D} RUN_MODE=${RUN_MODE} EK=${EK_STASH}"

# 关闭 golden 对拍；ss/ct 由 decaps 步与 device 闭环校验
(cd "${ENCAPS_DIR}" && SEED_D="${SEED_D}" \
    EK_KEM_SRC="${EK_STASH}" \
    KEM_ENCAPS_VERIFY=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")

C_OUT="${ENCAPS_DIR}/output/c.bin"
K_OUT="${ENCAPS_DIR}/output/K.bin"
C_SZ=$(wc -c <"${C_OUT}")
K_SZ=$(wc -c <"${K_OUT}")
if [ "${C_SZ}" -ne 1568 ] || [ "${K_SZ}" -ne 32 ]; then
    echo "[roundtrip_kem_encaps] ERROR: 输出尺寸 c=${C_SZ} K=${K_SZ}" >&2
    exit 1
fi

cp -f "${C_OUT}" "${STASH_DIR}/c.bin"
cp -f "${K_OUT}" "${STASH_DIR}/K_enc.bin"

echo "[roundtrip_kem_encaps] OK stash: ${STASH_DIR}/c.bin (1568B) + K_enc.bin (32B)"
