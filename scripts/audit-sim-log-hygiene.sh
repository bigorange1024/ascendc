#!/usr/bin/env bash
# 审计 SIM/CPU 工作路径与幽灵用例目录（重命名后残留 CAMODEL_LOG_PATH 导致）。
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

fail=0
warn=0

_fail() { echo "[FAIL] $*"; fail=$((fail + 1)); }
_warn() { echo "[WARN] $*"; warn=$((warn + 1)); }
_ok() { echo "[ OK ] $*"; }

echo "=== audit-sim-log-hygiene (${REPO_ROOT}) ==="

# 1) 已知幽灵目录（KeyGen 旧名，应只有 pass- 前缀）
if [[ -d ascendc-tests/fix-f203-alg13-device-keygen-k4 ]]; then
  _fail "ascendc-tests/fix-f203-alg13-device-keygen-k4 仍存在（应为 pass-fix-... 或已删除）"
else
  _ok "无 fix-f203-alg13-device-keygen-k4 幽灵目录"
fi

# 2) ascendc-tests 下仅含 sim_log、无源码的目录
while IFS= read -r -d '' d; do
  base="$(basename "${d}")"
  [[ "${base}" == backup ]] && continue
  parent="${d%/sim_log}"
  has_src=0
  for f in run.sh CMakeLists.txt main.cpp main_keygen.cpp; do
    [[ -f "${parent}/${f}" ]] && has_src=1 && break
  done
  if [[ "${has_src}" -eq 0 ]]; then
    n="$(find "${d}" -type f 2>/dev/null | wc -l)"
    _warn "无源码目录含 sim_log: ${parent} (${n} 个日志文件)"
  fi
done < <(find ascendc-tests -maxdepth 2 -type d -name sim_log -print0 2>/dev/null)

# 3) camodel_sim_log.sh 不得保留旧路径
if grep -q 'CAMODEL_LOG_PATH:-' scripts/camodel_sim_log.sh; then
  _fail "scripts/camodel_sim_log.sh 仍使用 \${CAMODEL_LOG_PATH:-...}（会保留 shell 残留）"
else
  _ok "camodel_sim_log.sh 强制绑定用例目录"
fi

# 4) kernel-run-timeout 须在 exec 前 source camodel
if ! grep -q 'camodel_sim_log.sh' scripts/kernel-run-timeout.sh; then
  _fail "scripts/kernel-run-timeout.sh 未 source camodel_sim_log.sh"
else
  _ok "kernel-run-timeout.sh 启动 kernel 前绑定工作目录"
fi

# 5) sim_env 清残留
if ! grep -q 'unset CAMODEL_LOG_PATH' scripts/sim_env.sh; then
  _fail "scripts/sim_env.sh 未 unset CAMODEL_LOG_PATH/ASCEND_WORK_PATH"
else
  _ok "sim_env_export 清除残留工作路径"
fi

# 6) 用例根目录 stray dump（应在 sim_log/ 内）
shopt -s nullglob
for root in ascendc-tests/*/ examples/incubating/*/; do
  [[ -d "${root}" ]] || continue
  [[ "${root}" == *"/backup/"* ]] && continue
  for stray in "${root}"/core*.dump "${root}"/profile_*_log*.toml; do
    [[ -e "${stray}" ]] || continue
    _warn "stray dump 在用例根: ${stray}"
  done
  for stray in "${root}"/cceprint "${root}"/npuchk; do
    [[ -e "${stray}" ]] || continue
    _warn "stray 目录在用例根: ${stray}"
  done
done
shopt -u nullglob

# 7) 残留 env 绑定测试
_stale="/tmp/audit-stale-sim-log-$$"
export CAMODEL_LOG_PATH="${_stale}"
export ASCEND_WORK_PATH="${_stale}"
# shellcheck source=/dev/null
source scripts/camodel_sim_log.sh "${REPO_ROOT}/ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg8-cbd-eta2-k4"
if [[ "${CAMODEL_LOG_PATH}" == "${_stale}" ]]; then
  _fail "source camodel_sim_log.sh 未能覆盖 stale CAMODEL_LOG_PATH"
elif [[ "${CAMODEL_LOG_PATH}" != "${REPO_ROOT}/ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg8-cbd-eta2-k4/sim_log" ]]; then
  _fail "camodel 绑定路径异常: ${CAMODEL_LOG_PATH}"
else
  _ok "stale env 覆盖测试通过"
fi
unset CAMODEL_LOG_PATH ASCEND_WORK_PATH ASCEND_PROCESS_LOG_PATH

echo "=== 结果: fail=${fail} warn=${warn} ==="
[[ "${fail}" -eq 0 ]]
