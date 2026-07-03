#!/usr/bin/env bash
# liboqs_kem_keygen_batch.sh — KEM.KeyGen 旁路 A 批测（默认 quiet，镜像 PKE kat）
#
# 默认 CPU×10 + SIM×1；终端仅进度行，详细 log 写入 output/liboqs_kem_keygen/kat.log。
#
# Usage：
#   bash scripts/liboqs_kem_keygen_batch.sh
#   bash scripts/liboqs_kem_keygen_batch.sh -r cpu
#   KEM_KG_CPU_TRIALS=3 KEM_KG_SIM_TRIALS=0 bash scripts/liboqs_kem_keygen_batch.sh
#   KEM_KG_VERBOSE=1 bash scripts/liboqs_kem_keygen_batch.sh   # 全量 log（旧行为）
#
# 环境（可选）：
#   KEM_KG_CPU_TRIALS / KEM_KG_SIM_TRIALS  默认 10 / 1
#   KEM_KG_VERBOSE=1                       关闭 quiet，run.sh 输出到终端
#   KEM_KG_LOG                             默认 output/liboqs_kem_keygen/kat.log
#   SOC_VERSION                            默认 Ascend910B4

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
    *) echo "[kg_batch] unknown option $1" >&2; exit 1 ;;
    esac
done

export KEM_KG_ONLY_MODE="${ONLY_MODE}"
exec python3 "${REPO_ROOT}/scripts/kat_liboqs_kem_keygen.py" "$@"
