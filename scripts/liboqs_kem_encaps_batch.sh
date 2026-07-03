#!/usr/bin/env bash
# liboqs_kem_encaps_batch.sh — KEM Encaps 分项 kat（固定 ek，随机 m，默认 quiet）
#
# Usage:
#   bash scripts/kem_keypair_stash_bootstrap.sh   # 一次性
#   bash scripts/liboqs_kem_encaps_batch.sh
#   KEM_ENC_CPU_TRIALS=3 bash scripts/liboqs_kem_encaps_batch.sh -r cpu
#   KEM_ENC_VERBOSE=1 bash scripts/liboqs_kem_encaps_batch.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ONLY_MODE=""
SOC_VERSION="${SOC_VERSION:-Ascend910B4}"
export SOC_VERSION

SHORT=r:,v:
LONG=run-mode:,soc-version:
OPTS=$(getopt -a --options "$SHORT" --longoptions "$LONG" -- "$@")
eval set -- "$OPTS"
while :; do
    case "$1" in
    -r | --run-mode) ONLY_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; export SOC_VERSION; shift 2 ;;
    --) shift; break ;;
    *) echo "[enc_batch] unknown option $1" >&2; exit 1 ;;
    esac
done

export KEM_ENC_ONLY_MODE="${ONLY_MODE}"
exec python3 "${REPO_ROOT}/scripts/kat_liboqs_kem_encaps.py" "$@"
