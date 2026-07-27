#!/usr/bin/env bash
# build-liboqs.sh — 在 thirdparty/liboqs 上配置并编译共享库（幂等）
#
# 供 clone-thirdparty.sh / Cloud Agent / 本地新机在 clone 后立刻可用 golden·KAT。
# 权威清单：docs/engineering/thirdparty-本地依赖.md
#
# Usage:
#   bash scripts/build-liboqs.sh
#   FORCE=1 bash scripts/build-liboqs.sh          # 强制重配+重编
#   LIBOQS_JOBS=4 bash scripts/build-liboqs.sh
#
# 产出：
#   thirdparty/liboqs/build/lib/liboqs.so
#   thirdparty/liboqs/build/include/…
#
# 随后可选：
#   bash scripts/build_liboqs_kem_ref.sh
#   bash scripts/build_liboqs_pke_ref_mlkem1024.sh   # 或兼容入口 build_liboqs_pke_ref.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LIBOQS_ROOT="${LIBOQS_ROOT:-${REPO_ROOT}/thirdparty/liboqs}"
LIBOQS_BUILD="${LIBOQS_BUILD:-${LIBOQS_ROOT}/build}"
FORCE="${FORCE:-0}"
JOBS="${LIBOQS_JOBS:-${CMAKE_BUILD_JOBS:-2}}"

if [[ ! -d "${LIBOQS_ROOT}" ]]; then
  echo "[build-liboqs] missing ${LIBOQS_ROOT}" >&2
  echo "[build-liboqs] 先跑: bash scripts/clone-thirdparty.sh" >&2
  exit 1
fi

# 粗检 tag（非强制失败：允许用户自管源树）
if [[ -d "${LIBOQS_ROOT}/.git" ]]; then
  tag="$(cd "${LIBOQS_ROOT}" && git describe --tags --exact-match 2>/dev/null || true)"
  if [[ -n "${tag}" && "${tag}" != "0.15.0" ]]; then
    echo "[build-liboqs] WARN: expected tag 0.15.0, got ${tag}" >&2
  fi
fi

need=1
if [[ "${FORCE}" != "1" ]] && [[ -f "${LIBOQS_BUILD}/lib/liboqs.so" || -f "${LIBOQS_BUILD}/lib/liboqs.a" ]]; then
  echo "[build-liboqs] skip: already built (${LIBOQS_BUILD}/lib/liboqs.{so,a}); FORCE=1 to rebuild"
  need=0
fi

if [[ "${need}" = "1" ]]; then
  echo "[build-liboqs] configure ${LIBOQS_ROOT} → ${LIBOQS_BUILD} (jobs=${JOBS})"
  cmake -S "${LIBOQS_ROOT}" -B "${LIBOQS_BUILD}" \
    -DBUILD_SHARED_LIBS=ON \
    -DOQS_BUILD_ONLY_LIB=ON \
    -DCMAKE_BUILD_TYPE=Release
  cmake --build "${LIBOQS_BUILD}" -j"${JOBS}"
fi

if [[ ! -f "${LIBOQS_BUILD}/lib/liboqs.so" && ! -f "${LIBOQS_BUILD}/lib/liboqs.a" ]]; then
  echo "[build-liboqs] ERROR: build finished but liboqs.{so,a} missing under ${LIBOQS_BUILD}/lib" >&2
  exit 1
fi

# 默认顺手编 KEM/PKE 黑盒 ref（golden / KAT）；可用 BUILD_LIBOQS_REFS=0 跳过
BUILD_REFS="${BUILD_LIBOQS_REFS:-1}"
if [[ "${BUILD_REFS}" = "1" ]]; then
  if [[ -f "${SCRIPT_DIR}/build_liboqs_kem_ref.sh" ]]; then
    bash "${SCRIPT_DIR}/build_liboqs_kem_ref.sh"
  fi
  if [[ -f "${SCRIPT_DIR}/build_liboqs_pke_ref_mlkem1024.sh" ]]; then
    bash "${SCRIPT_DIR}/build_liboqs_pke_ref_mlkem1024.sh"
  elif [[ -f "${SCRIPT_DIR}/build_liboqs_pke_ref.sh" ]]; then
    bash "${SCRIPT_DIR}/build_liboqs_pke_ref.sh"
  fi
fi

echo "[build-liboqs] OK: ${LIBOQS_BUILD}/lib/"
ls -la "${LIBOQS_BUILD}/lib"/liboqs.* 2>/dev/null || true
