#!/usr/bin/env bash
# ensure_thirdparty_dep.sh — 探针 run.sh 用：缺本地 thirdparty 依赖时按需 clone。
#
# 用法：
#   source "${REPO_ROOT}/scripts/ensure_thirdparty_dep.sh"
#   ensure_thirdparty_dep tiny_sha3
#   ensure_thirdparty_dep tiny_sha3 sha3.c   # 额外检查文件相对 thirdparty/<name>/
#
# 注意：本文件**只定义函数**，禁止 set -u/-e（避免污染调用方 run.sh）。
# Cloud：公开仓可直接 clone；ntt_onnx 须 ASCENDC_GH_PAT。

ensure_thirdparty_dep() {
    local name="${1:?need dep name under thirdparty/}"
    local check_rel="${2:-}"
    if [ -z "${REPO_ROOT:-}" ]; then
        echo "[deps] ERROR: REPO_ROOT must be set before ensure_thirdparty_dep" >&2
        return 1
    fi
    local dest="${REPO_ROOT}/thirdparty/${name}"
    local need_clone=0
    if [ ! -d "${dest}" ]; then
        need_clone=1
    fi
    if [ -n "${check_rel}" ] && [ ! -f "${dest}/${check_rel}" ]; then
        need_clone=1
    fi
    if [ "${need_clone}" -eq 0 ]; then
        return 0
    fi
    echo "[deps] missing thirdparty/${name}${check_rel:+ (${check_rel})}; running clone-thirdparty.sh ONLY=${name}"
    ONLY="${name}" BUILD_LIBOQS=0 bash "${REPO_ROOT}/scripts/clone-thirdparty.sh"
    if [ -n "${check_rel}" ] && [ ! -f "${dest}/${check_rel}" ]; then
        echo "[deps] ERROR: still missing ${dest}/${check_rel} after clone" >&2
        return 1
    fi
}
