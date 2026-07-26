#!/usr/bin/env bash
# vendor_sync.sh — D15 k3 自包含检查（不从 k4/frozen 覆盖本目录）
#
# 本 exp 由活跃 D15 k3 基线复制后自包含到 ML-KEM-768 incubating；关键几何（k=3、du=10/dv=4、AIV 2+1）
# 已写入本目录源码。run.sh 保留该入口，是为了兼容 k4 脚本结构；这里仅确认必需文件存在，
# 禁止运行时再从 k4 stable/probe 同步导致锁定参数回退。
set -euo pipefail
CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
need=(
  "compute/ntt_u/f203_decrypt_ntt_u_tiling.h"
  "compute/ntt_u/aiv_func.hpp"
  "compute/ntt_u/aic_func.hpp"
  "compute/alg11/multiply_ntts_ub.hpp"
  "compute/su_dot/innerproduct_mod.hpp"
  "compute/ntt_u/thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h"
)
for f in "${need[@]}"; do
  if [ ! -f "${CASE_DIR}/${f}" ]; then
    echo "[vendor_sync] missing ${f}" >&2
    exit 1
  fi
done
echo "[vendor_sync] self-contained k3 sources OK"
