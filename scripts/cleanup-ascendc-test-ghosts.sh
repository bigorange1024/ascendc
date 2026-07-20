#!/usr/bin/env bash
# 清理 ascendc-tests/ 下「更名幽灵」：无 run.sh/STATUS/源码、仅剩 build/I/O 产物的空壳目录。
#
# 成因：git mv fix-* → pass-fix-* 只搬已跟踪文件；旧路径下未跟踪的 build_*/input/output
# 会留下幽灵目录（2026-07-20 Decaps device 案例）。
#
# 用法（工程根）：
#   bash scripts/cleanup-ascendc-test-ghosts.sh          # 仅列出
#   bash scripts/cleanup-ascendc-test-ghosts.sh --delete # 删除空壳
#
# 另：禁止出现的误名（若存在且无权威源码则一并删）
#   pass-probe-*
#   fix-f203-alg21-kem-decaps-device-k4（已更名为 pass-fix-…）

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TESTS="${ROOT}/ascendc-tests"
DELETE=0
[[ "${1:-}" == "--delete" ]] && DELETE=1

has_source() {
  local d="$1"
  [[ -f "${d}/run.sh" || -f "${d}/STATUS.md" || -f "${d}/CMakeLists.txt" || -f "${d}/main.cpp" ]] && return 0
  # 常见 KEM/PKE 入口
  compgen -G "${d}/main_*.cpp" >/dev/null 2>&1 && return 0
  return 1
}

is_buildish() {
  local d="$1"
  [[ -d "${d}/build" || -d "${d}/build_prod_cpu" || -d "${d}/build_prod_sim" \
    || -d "${d}/input" || -d "${d}/output" || -d "${d}/sim_log" \
    || -d "${d}/out_prod_cpu" || -d "${d}/out_prod_sim" \
    || -d "${d}/cceprint" || -d "${d}/npuchk" || -d "${d}/golden" ]]
}

echo "[cleanup-ghosts] scan ${TESTS}"
found=0

# 1) 显式禁名（已退役 / 误名）
BANNED=(
  "fix-f203-alg21-kem-decaps-device-k4"
)
shopt -s nullglob
for d in "${TESTS}"/pass-probe-*; do
  BANNED+=("$(basename "$d")")
done
shopt -u nullglob

for name in "${BANNED[@]}"; do
  d="${TESTS}/${name}"
  [[ -e "$d" ]] || continue
  if has_source "$d"; then
    echo "[WARN] banned name but has source — refuse delete: ${name}"
    continue
  fi
  found=1
  if [[ "$DELETE" -eq 1 ]]; then
    rm -rf "$d"
    echo "[DELETED] ${name}"
  else
    echo "[GHOST] ${name} (banned leftover; run with --delete)"
  fi
done

# 2) 通用：无源码 + 仅构建产物
for d in "${TESTS}"/*/; do
  name="$(basename "$d")"
  [[ "$name" == "frozen" ]] && continue
  has_source "$d" && continue
  is_buildish "$d" || continue
  found=1
  if [[ "$DELETE" -eq 1 ]]; then
    rm -rf "$d"
    echo "[DELETED] ${name}"
  else
    echo "[GHOST] ${name} (build-only; run with --delete)"
  fi
done

if [[ "$found" -eq 0 ]]; then
  echo "[cleanup-ghosts] none"
else
  [[ "$DELETE" -eq 1 ]] || echo "[cleanup-ghosts] dry-run done; pass --delete to remove"
fi
