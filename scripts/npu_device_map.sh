#!/usr/bin/env bash
# npu_device_map.sh — 借入 8 卡机（逻辑设备 0–7）按用例树分配默认 ASCEND_DEVICE_ID。
#
# 2026-08-18 用户锁定（隔离同卡污染：探针卡死勿脏到 stable）：
#   examples/stable/**     → 1 号卡（ASCEND_DEVICE_ID=1）
#   examples/**（其余，含 incubating） → 2 号卡
#   ascendc-tests/**       → 3 号卡
#   其它路径               → 0 号卡
#
# SIM / CAModel 仍强制设备 0（本脚本只用于 -r npu）。
# 显式 `export ASCEND_DEVICE_ID=N` 始终优先，本函数不覆盖。
#
# 覆盖默认（可选）：
#   NPU_DEVICE_STABLE=1 NPU_DEVICE_EXAMPLES=2 NPU_DEVICE_TESTS=3 NPU_DEVICE_OTHER=0
#
# 用法：
#   source "${REPO_ROOT}/scripts/npu_device_map.sh"
#   npu_device_map_apply "/path/to/case"     # 未设置时 export
#   npu_device_id_for_path "/path/to/case"   # 只打印数字，不 export
#   bash scripts/npu_device_map.sh --self-test

npu_device_stable() { printf '%s\n' "${NPU_DEVICE_STABLE:-1}"; }
npu_device_examples() { printf '%s\n' "${NPU_DEVICE_EXAMPLES:-2}"; }
npu_device_tests() { printf '%s\n' "${NPU_DEVICE_TESTS:-3}"; }
npu_device_other() { printf '%s\n' "${NPU_DEVICE_OTHER:-0}"; }

# 规范化路径（尽量绝对路径，失败则原样）。
npu_device_norm_path() {
    local p="${1:-.}"
    if [ -d "${p}" ]; then
        (cd "${p}" && pwd)
    elif [ -f "${p}" ]; then
        (cd "$(dirname "${p}")" && pwd)
    else
        printf '%s\n' "${p}"
    fi
}

# stdout: stable | examples | tests | other
npu_device_class_for_path() {
    local p
    p="$(npu_device_norm_path "${1:-.}")"
    case "${p}" in
    */examples/stable/* | */examples/stable)
        printf '%s\n' stable
        ;;
    */examples/* | */examples)
        printf '%s\n' examples
        ;;
    */ascendc-tests/* | */ascendc-tests)
        printf '%s\n' tests
        ;;
    */graph-tests/* | */graph-tests)
        # toys / enc_related：与 ascendc-tests 同卡，避免污染 stable
        printf '%s\n' tests
        ;;
    *)
        printf '%s\n' other
        ;;
    esac
}

npu_device_id_for_path() {
    local class
    class="$(npu_device_class_for_path "${1:-.}")"
    case "${class}" in
    stable) npu_device_stable ;;
    examples) npu_device_examples ;;
    tests) npu_device_tests ;;
    *) npu_device_other ;;
    esac
}

# 未设置 ASCEND_DEVICE_ID 时按路径 export；已设置则保留并打日志。
npu_device_map_apply() {
    local dir="${1:-.}"
    local class id
    class="$(npu_device_class_for_path "${dir}")"
    id="$(npu_device_id_for_path "${dir}")"
    if [ -n "${ASCEND_DEVICE_ID+x}" ]; then
        echo "[npu_device_map] keep ASCEND_DEVICE_ID=${ASCEND_DEVICE_ID} (explicit; map would be ${class}→${id})"
        return 0
    fi
    export ASCEND_DEVICE_ID="${id}"
    echo "[npu_device_map] ${class} → ASCEND_DEVICE_ID=${id}  dir=$(npu_device_norm_path "${dir}")"
}

npu_device_map_self_test() {
    local root fail=0
    root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    _expect() {
        local path="$1" want_class="$2" want_id="$3"
        local got_c got_i
        got_c="$(npu_device_class_for_path "${path}")"
        got_i="$(npu_device_id_for_path "${path}")"
        if [ "${got_c}" != "${want_class}" ] || [ "${got_i}" != "${want_id}" ]; then
            echo "[FAIL] ${path} → class=${got_c} id=${got_i} (want ${want_class}/${want_id})" >&2
            fail=1
        else
            echo "[OK]   ${path} → ${got_c}/${got_i}"
        fi
    }
    _expect "${root}/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4" stable 1
    _expect "${root}/examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-encaps-k4" examples 2
    _expect "${root}/examples/incubating/ml-kem/ml-kem-512/exp-fips203-mlkem-pke-encrypt-k2" examples 2
    _expect "${root}/ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg20-kem-encaps-device-k4" tests 3
    _expect "${root}/graph-tests/toys/T01-mix-ntt13-handshake" tests 3
    _expect "${root}/ascendc-tests/add_custom" tests 3
    _expect "${root}/scripts" other 0
    # 显式覆盖不被 apply 改掉
    local saved="${ASCEND_DEVICE_ID-}"
    local had=0
    if [ -n "${ASCEND_DEVICE_ID+x}" ]; then had=1; fi
    unset ASCEND_DEVICE_ID || true
    export ASCEND_DEVICE_ID=5
    npu_device_map_apply "${root}/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4" >/dev/null
    if [ "${ASCEND_DEVICE_ID}" != "5" ]; then
        echo "[FAIL] explicit ASCEND_DEVICE_ID=5 was overwritten → ${ASCEND_DEVICE_ID}" >&2
        fail=1
    else
        echo "[OK]   explicit ASCEND_DEVICE_ID=5 preserved"
    fi
    unset ASCEND_DEVICE_ID || true
    if [ "${had}" = "1" ]; then export ASCEND_DEVICE_ID="${saved}"; fi
    if [ "${fail}" != "0" ]; then
        echo "[npu_device_map] self-test FAILED" >&2
        return 1
    fi
    echo "[npu_device_map] self-test PASS"
    return 0
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    set -euo pipefail
    case "${1:-}" in
    --self-test | self-test)
        npu_device_map_self_test
        ;;
    *)
        echo "usage: $0 --self-test" >&2
        echo "  or: source $0 && npu_device_map_apply <case_dir>" >&2
        exit 1
        ;;
    esac
fi
