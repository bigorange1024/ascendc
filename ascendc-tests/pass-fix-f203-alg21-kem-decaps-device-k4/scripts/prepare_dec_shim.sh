#!/usr/bin/env bash
# prepare_dec_shim.sh — 从 stable Decrypt 生成 SIM 合库用头隔离树（幂等）
#
# 背景（T2）：ascendc_library precompile 只认 target 级 -I，无法 per-TU 隔离；
# stable Decrypt/Encrypt 有大量同名头且内容分歧。合进单设备库前，须把 Decrypt
# 侧冲突 basename 改名为 dec_*，并改写 shim 内 #include。
#
# 源：examples/stable/stable-fips203-mlkem-pke-decrypt-k4（编译期权威；禁止 frozen）
# 目标：本用例 shim/pke_decrypt/（生成物，可 rm -rf 后重跑本脚本）
#
# 不改 Alg.18 语义；不修改 stable 源码。

set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "${CASE_DIR}/../.." && pwd)"
SRC="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-pke-decrypt-k4"
DST="${CASE_DIR}/shim/pke_decrypt"

if [[ ! -d "${SRC}/compute/g4_full" ]]; then
    echo "[dec_shim] ERROR: missing stable Decrypt at ${SRC}" >&2
    exit 1
fi

rm -rf "${DST}"
mkdir -p "${DST}"

# 仅同步 fused entry 可达子树（与 CMake _DECRYPT_INCLUDES 对齐）
# Cloud VM 可能无 rsync：用 cp -a 整目录复制（再靠改名/改写做隔离）
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
# 根布局头（fused / su_dot / unpack 共用）
cp -a "${SRC}/f203_decrypt_layout.h" "${DST}/f203_decrypt_layout.h"

# 去掉 stable 目录里可能存在的 thirdparty 软链/目录（合库不需要）
find "${DST}" -name thirdparty -exec rm -rf {} + 2>/dev/null || true
# 非 fused 的独立 entry 不进 KERNEL_FILES；删以免误用
rm -f "${DST}/compute/ntt_u/f203_decrypt_ntt_u_entry.cpp"
rm -f "${DST}/compute/su_dot/f203_decrypt_su_dot_kernel.cpp"

# 与 stable Encrypt 同名且可能内容分歧的 basename → dec_*（合库单 -I 必需）
# 含 .cpp：su_dot_impl 以 #include "alg11_rom_tables.cpp" 拉入 ROM 定义
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

# 改写 shim 内对冲突头的 #include（幂等：已是 dec_ 的不再叠前缀）
rewrite_includes() {
    local name="$1"
    local f
    while IFS= read -r -d '' f; do
        # 仅改 "name"；跳过已是 "dec_name"
        sed -i \
            -e "s|#include \"${name}\"|#include \"dec_${name}\"|g" \
            -e "s|#include \"dec_dec_${name}\"|#include \"dec_${name}\"|g" \
            "${f}"
    done < <(find "${DST}" -type f \( -name '*.hpp' -o -name '*.h' -o -name '*.cpp' \) -print0)
}

for _n in "${CONFLICTS[@]}"; do
    rewrite_includes "${_n}"
done

# 护栏：shim 内不应再残留未改名的冲突 basename 文件
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

echo "[dec_shim] OK ${DST} (from stable Decrypt; conflict headers → dec_*)"
