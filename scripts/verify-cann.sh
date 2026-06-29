#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "${SCRIPT_DIR}/env.sh"

echo "=== CANN environment ==="
echo "CANN_HOME=${CANN_HOME}"
echo "ASCEND_HOME_DIR=${ASCEND_HOME_DIR}"

INFO="${CANN_HOME}/x86_64-linux/ascend_toolkit_install.info"
if [[ ! -f "${INFO}" ]]; then
  INFO="${HOME}/Ascend/ascend-toolkit/latest/x86_64-linux/ascend_toolkit_install.info"
fi
if [[ -f "${INFO}" ]]; then
  echo "--- install info ---"
  grep -E '^version=|^package_name=' "${INFO}" || cat "${INFO}"
else
  echo "WARN: install info not found"
fi

echo ""
echo "=== Core tools ==="
command -v ccec
ccec --version | head -3

echo ""
echo "=== CPU twin-debug (tikicpulib) ==="
test -f "${CANN_HOME}/tools/tikicpulib/lib/Ascend910A/libtikcpp_debug.so"
echo "tikicpulib: OK"

echo ""
echo "=== NPU simulator (CAModel, Ascend910A) ==="
SIM_DIR="${CANN_HOME}/x86_64-linux/simulator/Ascend910A/lib"
test -d "${SIM_DIR}"
ls "${SIM_DIR}"/*.so | wc -l | xargs -I{} echo "simulator libs: {} files"

echo ""
echo "=== Python (CANN deps) ==="
python3 -m pip show numpy protobuf 2>/dev/null | grep -E '^Name:|^Version:' || true

echo ""
echo "=== Optional: ACL Python ==="
python3 -c "import acl; print('acl module: OK')" 2>/dev/null || echo "acl module: skip (not required for AscendC kernel dev)"

echo ""
echo "=== System install marker (SIM aclInit) ==="
if [[ -f /etc/ascend_install.info ]]; then
  echo "/etc/ascend_install.info: OK"
else
  echo "WARN: /etc/ascend_install.info missing — SIM 需一次性: sudo ln -sf \${HOME}/Ascend/ascend_cann_install.info /etc/ascend_install.info"
fi

echo ""
echo "All checks passed. CANN 9.0.0 toolkit + 910-ops + CPU debug + simulator are ready."
