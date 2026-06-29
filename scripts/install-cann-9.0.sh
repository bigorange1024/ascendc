#!/usr/bin/env bash
# Install CANN Community 9.0.0 on non-Ascend x86_64 (WSL).
# Requires: ~4GB download, ~10GB disk under $HOME/Ascend
set -euo pipefail

CANN_VERSION="9.0.0"
ARCH="x86_64"
OBS_BASE="https://ascend-repo.obs.cn-east-2.myhuaweicloud.com/CANN/CANN%209.0.0"
PKG_DIR="${HOME}/ascendc/packages"
TOOLKIT_RUN="Ascend-cann-toolkit_${CANN_VERSION}_linux-${ARCH}.run"
OPS_RUN="Ascend-cann-910-ops_${CANN_VERSION}_linux-${ARCH}.run"

mkdir -p "${PKG_DIR}"
cd "${PKG_DIR}"

download_if_missing() {
  local name="$1"
  local url="$2"
  if [[ -f "${name}" ]]; then
    echo "[skip] ${name} already exists"
    return 0
  fi
  echo "[download] ${url}"
  wget -c --progress=dot:giga "${url}" -O "${name}"
}

echo "=== Python deps ==="
export PATH="${HOME}/.local/bin:${PATH}"
if ! python3 -m pip --version &>/dev/null; then
  curl -sS https://bootstrap.pypa.io/get-pip.py -o /tmp/get-pip.py
  python3 /tmp/get-pip.py --user
fi
python3 -m pip install --user \
  attrs cython 'numpy>=1.19.2,<=2.0' decorator sympy cffi pyyaml \
  pathlib2 psutil 'protobuf>=3.20,<4' scipy requests absl-py

echo "=== Download CANN 9.0.0 (official OBS) ==="
download_if_missing "${TOOLKIT_RUN}" "${OBS_BASE}/${TOOLKIT_RUN}"
download_if_missing "${OPS_RUN}" "${OBS_BASE}/${OPS_RUN}"

echo "=== Install Toolkit ==="
chmod +x "${TOOLKIT_RUN}"
./"${TOOLKIT_RUN}" --install --quiet --feature=ascendc

echo "=== Install 910-ops ==="
chmod +x "${OPS_RUN}"
./"${OPS_RUN}" --install --quiet

# Detect set_env (9.0 may use ascend-toolkit or cann path)
SET_ENV=""
for candidate in \
  "${HOME}/Ascend/cann/set_env.sh" \
  "${HOME}/Ascend/ascend-toolkit/set_env.sh" \
  "/usr/local/Ascend/cann/set_env.sh"; do
  if [[ -f "${candidate}" ]]; then
    SET_ENV="${candidate}"
    break
  fi
done

if [[ -z "${SET_ENV}" ]]; then
  echo "ERROR: set_env.sh not found after install"
  find "${HOME}/Ascend" -name set_env.sh 2>/dev/null || true
  exit 1
fi

MARKER="# Ascend CANN ${CANN_VERSION}"
BASHRC="${HOME}/.bashrc"
if ! grep -qF "${MARKER}" "${BASHRC}" 2>/dev/null; then
  cat >> "${BASHRC}" <<EOF

${MARKER}
source \${HOME}/ascendc/scripts/env.sh
EOF
fi

# shellcheck source=env.sh
source "${HOME}/ascendc/scripts/env.sh"
echo "CANN ${CANN_VERSION} installed. CANN_HOME=${CANN_HOME}"
which ccec 2>/dev/null && ccec --version | head -2
