#!/usr/bin/env bash
# build_liboqs_pke_ref.sh — 兼容入口（转发到 ML-KEM-1024 专用构建）
#
# 真名：build_liboqs_pke_ref_mlkem1024.sh / liboqs_pke_ref_mlkem1024.c
# 保留本文件以免旧文档与 clone-thirdparty 调用断裂。
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "${SCRIPT_DIR}/build_liboqs_pke_ref_mlkem1024.sh"
