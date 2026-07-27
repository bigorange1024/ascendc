#!/usr/bin/env bash
# vendor_sync.sh — D15 k2 自包含检查（不从 k3/k4/frozen 覆盖本目录）
#
# 本探针由活跃 768 k3 D15 复制后 retarget 到 ML-KEM-512；关键几何（k=2、du=10/dv=4、AIV 1+1）
# 已写入本目录源码。run.sh 保留该入口用于自包含检查；这里仅确认必需文件存在，
# 禁止运行时再从其它参数组 stable/probe 同步导致锁定参数回退。
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
echo "[vendor_sync] self-contained k2 sources OK"
