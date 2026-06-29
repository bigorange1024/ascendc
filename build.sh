#!/usr/bin/env bash
# 根目录 C 工程：不依赖 C/C++ Runner，终端或 Ctrl+Shift+B 均可
set -euo pipefail
cd "$(dirname "$0")"
make "$@"
