#!/usr/bin/env bash
# build_liboqs_pke_ref_mlkem1024.sh — 编译 ML-KEM-1024 专用 PKE 黑盒
#
# 源码 / 产物均带 mlkem1024 后缀，避免被误当成 512/768 通用 helper。
# 本阶段不对其他参数组做 PKE liboqs 交叉（见 512 计划 P0-G）。
#
# 链接说明（T18，2026-07-28 关闭）：
#   liboqs 以 -fvisibility=hidden 构建，mlkem-native 的
#   PQCP_MLKEM_NATIVE_MLKEM1024_C_indcpa_{enc,dec} 以及
#   OQS_SHA3_shake*_absorb_once 等 shim **不进** liboqs.so 的 dynsym，
#   仅 -loqs 无法解析 encrypt/decrypt（改名本身不是根因）。
#   解法：把 ml_kem_1024_ref 的相关 .o + pqclean fips202 shim .o
#   与 liboqs-internal.a 一并链入本可执行文件；KeyGen 仍走公开
#   OQS_KEM_ml_kem_1024_keypair_derand（-loqs）。
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LIBOQS_ROOT="${LIBOQS_ROOT:-${REPO_ROOT}/thirdparty/liboqs}"
LIBOQS_BUILD="${LIBOQS_BUILD:-${LIBOQS_ROOT}/build}"
OUT_BIN="${SCRIPT_DIR}/liboqs_pke_ref_mlkem1024"
LIBOQS_LIB="${LIBOQS_BUILD}/lib/liboqs.so"
LIBOQS_INTERNAL="${LIBOQS_BUILD}/lib/liboqs-internal.a"
NATIVE_ROOT="${LIBOQS_ROOT}/src/kem/ml_kem/mlkem-native_ml-kem-1024_ref"
OBJ_MLKEM="${LIBOQS_BUILD}/src/kem/ml_kem/CMakeFiles/ml_kem_1024_ref.dir"
OBJ_SHIM="${LIBOQS_BUILD}/src/common/CMakeFiles/common.dir/pqclean_shims"

die() { echo "[build_liboqs_pke_ref_mlkem1024] $*" >&2; exit 1; }

if [ ! -f "${LIBOQS_LIB}" ]; then
    if [ -f "${LIBOQS_BUILD}/lib/liboqs.a" ]; then
        LIBOQS_LIB="${LIBOQS_BUILD}/lib/liboqs.a"
    else
        die "missing ${LIBOQS_BUILD}/lib/liboqs.{so,a}（先 bash scripts/build-liboqs.sh）"
    fi
fi
[ -f "${LIBOQS_INTERNAL}" ] || die "missing ${LIBOQS_INTERNAL}"

# mlkem-native 1024_ref 对象（indcpa + 依赖；不含 kem.c——KeyGen 走 OQS 公开 API）
MLKEM_SRC_NAMES=(indcpa.c compress.c poly.c poly_k.c sampling.c verify.c debug.c)
MLKEM_OBJS=()
for name in "${MLKEM_SRC_NAMES[@]}"; do
    obj="$(find "${OBJ_MLKEM}" -name "${name}.o" 2>/dev/null | head -n1 || true)"
    [ -n "${obj}" ] || die "missing ${name}.o under ${OBJ_MLKEM}（需完整 liboqs build 树，勿只拷 .so）"
    MLKEM_OBJS+=("${obj}")
done

SHIM_OBJS=(
    "${OBJ_SHIM}/fips202.c.o"
    "${OBJ_SHIM}/fips202x4.c.o"
)
for obj in "${SHIM_OBJS[@]}"; do
    [ -f "${obj}" ] || die "missing ${obj}（pqclean shim；需完整 liboqs build）"
done

# rpath：.so 场景；静态 .a 时仍无害
RPATH_FLAGS=()
if [[ "${LIBOQS_LIB}" == *.so ]]; then
    RPATH_FLAGS=(-Wl,-rpath,"${LIBOQS_BUILD}/lib")
fi

gcc -O2 -Wall -Wextra \
    -I"${LIBOQS_BUILD}/include" \
    -I"${LIBOQS_ROOT}/src/common/pqclean_shims" \
    -I"${NATIVE_ROOT}" \
    "${SCRIPT_DIR}/liboqs_pke_ref_mlkem1024.c" \
    "${MLKEM_OBJS[@]}" \
    "${SHIM_OBJS[@]}" \
    "${LIBOQS_INTERNAL}" \
    -L"${LIBOQS_BUILD}/lib" "${RPATH_FLAGS[@]}" -loqs \
    -lcrypto -lpthread \
    -o "${OUT_BIN}"

# 兼容旧路径名：软链 liboqs_pke_ref → 本产物（只读调用方可继续用旧名）
ln -sfn "$(basename "${OUT_BIN}")" "${SCRIPT_DIR}/liboqs_pke_ref"
echo "[build_liboqs_pke_ref_mlkem1024] OK: ${OUT_BIN} (+ symlink liboqs_pke_ref)"
