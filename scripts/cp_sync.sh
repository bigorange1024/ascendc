#!/usr/bin/env bash
# cp_sync.sh — 无 rsync 依赖的目录同步（Cloud 等精简环境常用）。
#
# 用法（等价于「rsync -a SRC/ DST/」或带 --delete）：
#   source "${REPO}/scripts/cp_sync.sh"
#   cp_sync_dir [--delete] SRC_DIR/ DST_DIR/
#   cp_sync_items [--delete] DST_DIR -- item1 item2 ...
#
# 说明：
#   - 有 rsync 时优先用它（行为与旧脚本一致）；否则 rm+cp -a
#   - 不含复杂 --exclude 列表：调用方自行限定要拷的项
set -euo pipefail

cp_sync_have_rsync() {
    command -v rsync >/dev/null 2>&1
}

# 同步「目录内容」SRC/ → DST/（两侧均目录）
cp_sync_dir() {
    local do_delete=0
    if [[ "${1:-}" == "--delete" ]]; then
        do_delete=1
        shift
    fi
    local src="${1:?cp_sync_dir: need SRC}"
    local dst="${2:?cp_sync_dir: need DST}"
    # 允许 SRC 带或不带尾 /
    src="${src%/}"
    if [[ ! -d "${src}" ]]; then
        echo "[cp_sync] ERROR: missing source dir ${src}" >&2
        return 1
    fi
    mkdir -p "${dst}"
    if cp_sync_have_rsync; then
        if [[ "${do_delete}" -eq 1 ]]; then
            rsync -a --delete "${src}/" "${dst}/"
        else
            rsync -a "${src}/" "${dst}/"
        fi
        return 0
    fi
    if [[ "${do_delete}" -eq 1 ]]; then
        find "${dst}" -mindepth 1 -maxdepth 1 -exec rm -rf {} +
    fi
    cp -a "${src}/." "${dst}/"
}

# 将若干文件/目录拷入 DST（可选先清空 DST 内一层）
cp_sync_items() {
    local do_delete=0
    if [[ "${1:-}" == "--delete" ]]; then
        do_delete=1
        shift
    fi
    local dst="${1:?cp_sync_items: need DST}"
    shift
    if [[ "${1:-}" == "--" ]]; then
        shift
    fi
    mkdir -p "${dst}"
    if [[ "${do_delete}" -eq 1 ]]; then
        find "${dst}" -mindepth 1 -maxdepth 1 -exec rm -rf {} +
    fi
    local item
    for item in "$@"; do
        if [[ ! -e "${item}" ]]; then
            echo "[cp_sync] ERROR: missing ${item}" >&2
            return 1
        fi
        cp -a "${item}" "${dst}/"
    done
}
