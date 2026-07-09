#!/usr/bin/env bash
# xelatex-clean.sh — 编译 .tex 两遍，成功后仅保留 .tex + .pdf（删中间文件）
# Usage: bash scripts/xelatex-clean.sh <path/to/file>.tex
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <file.tex>" >&2
  exit 2
fi

TEX_PATH="$(readlink -f "$1")"
if [ ! -f "${TEX_PATH}" ]; then
  echo "ERROR: not found: ${TEX_PATH}" >&2
  exit 1
fi

TEX_DIR="$(dirname "${TEX_PATH}")"
TEX_BASE="$(basename "${TEX_PATH}" .tex)"
PDF_PATH="${TEX_DIR}/${TEX_BASE}.pdf"

cd "${TEX_DIR}"
xelatex -interaction=nonstopmode "${TEX_BASE}.tex" >/tmp/xelatex-"${TEX_BASE}"-1.log 2>&1 || {
  echo "ERROR: xelatex pass 1 failed; see /tmp/xelatex-${TEX_BASE}-1.log" >&2
  tail -40 /tmp/xelatex-"${TEX_BASE}"-1.log >&2 || true
  exit 1
}
xelatex -interaction=nonstopmode "${TEX_BASE}.tex" >/tmp/xelatex-"${TEX_BASE}"-2.log 2>&1 || {
  echo "ERROR: xelatex pass 2 failed; see /tmp/xelatex-${TEX_BASE}-2.log" >&2
  tail -40 /tmp/xelatex-"${TEX_BASE}"-2.log >&2 || true
  exit 1
}

# 清理同主文件名中间产物
for ext in aux log toc out fls fdb_latexmk synctex.gz nav snm vrb; do
  rm -f "${TEX_DIR}/${TEX_BASE}.${ext}"
done

if [ ! -f "${PDF_PATH}" ]; then
  echo "ERROR: PDF not produced: ${PDF_PATH}" >&2
  exit 1
fi

echo "OK: ${PDF_PATH}"
