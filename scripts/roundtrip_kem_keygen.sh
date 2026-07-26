#!/usr/bin/env bash
# roundtrip_kem_keygen.sh — KEM 纯 device round-trip 第 1 步：仅产出密钥对（不测、不对拍）
#
# 作用：跑一次 Alg.19 KeyGen，将 ek/dk 写入 round-trip stash，供同 RUN_MODE 的 encaps/decaps 使用。
# 数据面：output/roundtrip_kem/<cpu|sim>/{ek_kem.bin, dk_kem.bin}
#
# Usage（CPU / SIM 分开跑，每次只指定一种 -r）：
#   bash scripts/roundtrip_kem_keygen.sh -r cpu -v Ascend910B4
#   bash scripts/roundtrip_kem_keygen.sh -r sim -v Ascend910B4
#
# 环境（可选）：
#   SEED_D=20260619
#   ROUNDTRIP_KEM_STASH=<dir>   默认 output/roundtrip_kem/${RUN_MODE}
#   KEM_KEYGEN_FORCE_REBUILD=1

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KEYGEN_DIR="${KEYGEN_DIR:-${REPO_ROOT}/ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4}"

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
    *) echo "[roundtrip_kem_keygen] unknown option $1" >&2; exit 1 ;;
    esac
done

if [ "${RUN_MODE}" != "cpu" ] && [ "${RUN_MODE}" != "sim" ]; then
    echo "[roundtrip_kem_keygen] ERROR: -r 须为 cpu 或 sim（一次只跑一种）" >&2
    exit 1
fi

STASH_DIR="${ROUNDTRIP_KEM_STASH:-${REPO_ROOT}/output/roundtrip_kem/${RUN_MODE}}"
mkdir -p "${STASH_DIR}"

echo "[roundtrip_kem_keygen] SEED_D=${SEED_D} RUN_MODE=${RUN_MODE} STASH=${STASH_DIR}"

# 生产路径 KeyGen；关闭 golden 对拍（round-trip 在 decaps 步验 ss 一致）
(cd "${KEYGEN_DIR}" && SEED_D="${SEED_D}" KEM_KEYGEN_VERIFY=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")

EK_OUT="${KEYGEN_DIR}/output/ek_kem.bin"
DK_OUT="${KEYGEN_DIR}/output/dk_kem.bin"
EK_SZ=$(wc -c <"${EK_OUT}")
DK_SZ=$(wc -c <"${DK_OUT}")
if [ "${EK_SZ}" -ne 1568 ] || [ "${DK_SZ}" -ne 3168 ]; then
    echo "[roundtrip_kem_keygen] ERROR: 密钥尺寸 ek=${EK_SZ} dk=${DK_SZ}" >&2
    exit 1
fi

cp -f "${EK_OUT}" "${STASH_DIR}/ek_kem.bin"
cp -f "${DK_OUT}" "${STASH_DIR}/dk_kem.bin"

echo "[roundtrip_kem_keygen] OK stash: ${STASH_DIR}/ek_kem.bin (1568B) + dk_kem.bin (3168B)"
