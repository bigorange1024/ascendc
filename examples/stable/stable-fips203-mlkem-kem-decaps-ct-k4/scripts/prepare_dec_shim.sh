#!/usr/bin/env bash
# prepare_dec_shim.sh — 从本目录 vendored Decrypt 生成 SIM 合库用头隔离树（幂等）
#
# 背景（T2）：ascendc_library precompile 只认 target 级 -I，无法 per-TU 隔离；
# Decrypt/Encrypt 有大量同名头且内容分歧。合进单设备库前，须把 Decrypt
# 侧冲突 basename 改名为 dec_*，并改写 shim 内 #include。
#
# 源：本用例 pke_decrypt/（vendored 自 stable Decrypt；禁止 frozen）
# 目标：本用例 shim/pke_decrypt/（生成物，可 rm -rf 后重跑本脚本）
#
# 不改 Alg.18 语义；不修改其它 examples 源码。

set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${CASE_DIR}/pke_decrypt"
DST="${CASE_DIR}/shim/pke_decrypt"

if [[ ! -d "${SRC}/compute/g4_full" ]]; then
    echo "[dec_shim] ERROR: missing vendored Decrypt at ${SRC}" >&2
    exit 1
fi

rm -rf "${DST}"
mkdir -p "${DST}"

_copy_tree() {
    local from="$1" to="$2"
    mkdir -p "${to}"
    cp -a "${from}/." "${to}/"
}
_copy_tree "${SRC}/compute/g4_full" "${DST}/compute/g4_full"
_copy_tree "${SRC}/compute/ntt_u" "${DST}/compute/ntt_u"
_copy_tree "${SRC}/compute/alg11" "${DST}/compute/alg11"
_copy_tree "${SRC}/compute/su_dot" "${DST}/compute/su_dot"
_copy_tree "${SRC}/compute/compress1_byteencode1" "${DST}/compute/compress1_byteencode1"
_copy_tree "${SRC}/unpack" "${DST}/unpack"
_copy_tree "${SRC}/prep/decode_dk" "${DST}/prep/decode_dk"
cp -a "${SRC}/f203_decrypt_layout.h" "${DST}/f203_decrypt_layout.h"

find "${DST}" -name thirdparty -exec rm -rf {} + 2>/dev/null || true
rm -f "${DST}/compute/ntt_u/f203_decrypt_ntt_u_entry.cpp"
rm -f "${DST}/compute/su_dot/f203_decrypt_su_dot_kernel.cpp"

CONFLICTS=(
    aic_func.hpp
    aiv_func.hpp
    basic.hpp
    kyber_limb6.hpp
    ntt_vec.hpp
    stage1_config.hpp
    stage3_config.hpp
    stage3_mod_variants.hpp
    alg11_fixed_n256.hpp
    alg11_ub_load.hpp
    alg11_vec_pipe.hpp
    alg11_gammas.h
    alg11_rom_tables.h
    alg11_rom_tables.cpp
    multiply_ntts_config.hpp
    multiply_ntts_ub.hpp
    multiply_ntts_vec.hpp
    tiling.h
)

rename_one() {
    local name="$1"
    local f
    while IFS= read -r -d '' f; do
        local dir base
        dir="$(dirname "${f}")"
        base="$(basename "${f}")"
        if [[ "${base}" == dec_* ]]; then
            continue
        fi
        mv -f "${f}" "${dir}/dec_${base}"
    done < <(find "${DST}" -type f -name "${name}" -print0 2>/dev/null)
}

for _n in "${CONFLICTS[@]}"; do
    rename_one "${_n}"
done

rewrite_includes() {
    local name="$1"
    local f
    while IFS= read -r -d '' f; do
        sed -i \
            -e "s|#include \"${name}\"|#include \"dec_${name}\"|g" \
            -e "s|#include \"dec_dec_${name}\"|#include \"dec_${name}\"|g" \
            "${f}"
    done < <(find "${DST}" -type f \( -name '*.hpp' -o -name '*.h' -o -name '*.cpp' \) -print0)
}

for _n in "${CONFLICTS[@]}"; do
    rewrite_includes "${_n}"
done

_bad=0
for _n in "${CONFLICTS[@]}"; do
    if find "${DST}" -type f -name "${_n}" | grep -q .; then
        echo "[dec_shim] ERROR: leftover unrenamed ${_n}" >&2
        _bad=1
    fi
done
if [[ ! -f "${DST}/compute/g4_full/f203_decrypt_device_fused_entry.cpp" ]]; then
    echo "[dec_shim] ERROR: missing fused entry in shim" >&2
    _bad=1
fi
if [[ ! -f "${DST}/compute/ntt_u/dec_aiv_func.hpp" ]]; then
    echo "[dec_shim] ERROR: missing dec_aiv_func.hpp" >&2
    _bad=1
fi
if [[ "${_bad}" -ne 0 ]]; then
    exit 1
fi

echo "[dec_shim] OK ${DST} (from vendored pke_decrypt; conflict headers → dec_*)"
