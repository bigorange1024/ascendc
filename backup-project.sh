#!/usr/bin/env bash
# ascendc 工程白名单备份（非全仓库镜像）
#
# 用法（工程根目录）：
#   bash backup-project.sh
#
# 备份前须先刷新各 INDEX.md、qa/TODO.md、README.md（见 .cursor/rules/ascendc-development.mdc §备份）
#
# 设计目标（2026-06-28 刷新）：
#   - 必须能恢复 exp/探针 的 prep/、compute/、cmake/、scripts/ 等 vendored 源码树
#   - 必须含 scripts/（sim_env.sh 等）与 AGENT_HANDOFF.md（每日交接）
#   - 排除 build/out/input/output 及二进制产物

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# 与历史快照一致：v0.1_YYYYMMDDHHMMSS（无日期/时间中间下划线）
BACKUP_NAME="v0.1_$(date +%Y%m%d%H%M%S)"
DEST="backup/$BACKUP_NAME"
mkdir -p "$DEST"

# rsync 排除：构建产物、I/O 数据、仿真 dump（保留源码与规格书）
RSYNC_EXCLUDES=(
  --exclude='build/'
  --exclude='build_*/'
  --exclude='out/'
  --exclude='out_*/'
  --exclude='input/'
  --exclude='output/'
  --exclude='dump/'
  --exclude='logs/'
  --exclude='log/'
  --exclude='sim_log/'
  --exclude='auto_gen/'
  --exclude='npuchk/'
  --exclude='cceprint/'
  --exclude='OPPROF_*/'
  --exclude='.cache/'
  --exclude='__pycache__/'
  --exclude='*.pyc'
  --exclude='.pydeps/'
  --exclude='*.o'
  --exclude='*.so'
  --exclude='*.a'
  --exclude='*.bin'
  --exclude='*.log'
  --exclude='*.toml'
  --exclude='compile_commands.json'
  --exclude='CMakeCache.txt'
  --exclude='CMakeFiles/'
  --exclude='*.ninja'
  --exclude='*.ninja_log'
  --exclude='*.ninja_deps'
  --exclude='*.stamp'
  --exclude='*.tmp'
  --exclude='*.aux'
  --exclude='*.fdb_latexmk'
  --exclude='*.fls'
  --exclude='*.out'
  --exclude='*.synctex.gz'
  --exclude='*.toc'
  --exclude='*.bbl'
  --exclude='*.blg'
  --exclude='*.xdv'
  --exclude='ascendc_keygen_bbit'
)

sync_tree() {
  local src="$1"
  local dst="$2"
  mkdir -p "$dst"
  rsync -a "${RSYNC_EXCLUDES[@]}" "$src/" "$dst/"
}

# --- 根文档与脚本 ---
for f in README.md AGENT_HANDOFF.md Makefile backup-project.sh; do
  if [ -f "$f" ]; then
    rsync -a "$f" "$DEST/"
  fi
done

# --- Cursor Rule / Skill（Agent 续修依赖）---
for d in .cursor/rules .cursor/skills; do
  if [ -d "$d" ]; then
    mkdir -p "$DEST/$(dirname "$d")"
    rsync -a "$d/" "$DEST/$d/"
  fi
done

# --- 核心目录（全树，artifact 排除）---
for d in qa library scripts src include; do
  if [ -d "$d" ]; then
    sync_tree "$d" "$DEST/$d"
  fi
done

# --- docs/：规格与 note（含 .tex/.pdf）---
if [ -d docs ]; then
  mkdir -p "$DEST/docs"
  rsync -a \
    --include='*/' \
    --include='*.md' \
    --include='*.tex' \
    --include='*.pdf' \
    --include='*.txt' \
    --exclude='*' \
    docs/ "$DEST/docs/"
fi

# --- 用例树：ascendc-tests/ + examples/（含 prep/compute/cmake/scripts/thirdparty vendored）---
for d in ascendc-tests examples; do
  if [ -d "$d" ]; then
    sync_tree "$d" "$DEST/$d"
  fi
done

# --- 根配置（若存在）---
for f in CMakeLists.txt CMakePresets.json .gitignore .clang-format .clangd; do
  if [ -f "$f" ]; then
    rsync -a "$f" "$DEST/"
  fi
done

# --- packages/ 仅索引与说明（不含 CANN 大包）---
if [ -d packages ]; then
  mkdir -p "$DEST/packages"
  rsync -a \
    --include='*/' \
    --include='*.md' \
    --include='*.txt' \
    --include='*.json' \
    --exclude='*' \
    packages/ "$DEST/packages/" 2>/dev/null || true
fi

COUNT=$(find "$DEST" -type f | wc -l)

cat > "$DEST/BACKUP_README.txt" << EOF
# ascendc 备份说明

- 版本前缀：v0.1_YYYYMMDDHHMMSS（与 2026-06-15 起历史快照一致；2026-06-28 起扩展 scripts/prep/compute 范围）
- 备份时间：$(date '+%Y-%m-%d %H:%M:%S')
- 文件数：${COUNT}
- 脚本：backup-project.sh（与快照同目录或工程根）

## 包含（相对旧 v0.1 快照新增/强调）

- scripts/（sim_env.sh、camodel_sim_log.sh、xelatex-clean.sh、inject_probe_code_comments.py 等）
- AGENT_HANDOFF.md、backup-project.sh
- .cursor/rules/、.cursor/skills/
- examples/、ascendc-tests/ 完整源码树：
  prep/、compute/、cmake/、scripts/prep、scripts/compute
  *-customspec.tex/.pdf、STATUS.md、INTEGRATION_PLAN.md、run.sh、*.hpp、*.cpp
  examples 内 vendored thirdparty/ntt_onnx/（Host LUT 表）
- library/（含 shared/shake_xof_kernel 等）
- qa/、docs/notes/、docs/engineering/、docs/specs/

## 不包含

- backup/（历史快照）、.git/
- build/、out/、input/、output/、sim_log/、auto_gen/、*.bin、*.log
- samples/、thirdparty/liboqs/（体积过大；KAT 需本机 clone liboqs）
- CANN 安装目录、编译产物 .o/.so

## 恢复步骤

1. 将本目录内容复制回工程根（勿覆盖 backup/ 子目录）
2. source ~/Ascend/cann/bin/setenv.bash（或 scripts/env.sh）
3. 进入用例目录：bash run.sh -r cpu -v Ascend910B4
4. 若缺 liboqs：仅 KAT 脚本需要，见 examples/.../scripts/build_liboqs_pke_ref.sh

## 生成

bash backup-project.sh
EOF

echo "备份完成: $DEST (${COUNT} 个文件)"
echo "说明: $DEST/BACKUP_README.txt"
