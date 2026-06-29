#!/usr/bin/env bash
# Install CANN 8.2.RC1 on non-Ascend x86_64 (WSL) for AscendC CPU debug + simulation.
set -euo pipefail

CANN_VERSION="8.2.RC1"
ARCH="x86_64"
REPO_BASE="https://repo.mindspore.cn/CANN/Community/${CANN_VERSION}"
PKG_DIR="${HOME}/ascendc/packages"
TOOLKIT_RUN="Ascend-cann-toolkit_${CANN_VERSION}_linux-${ARCH}.run"
KERNELS_RUN="Ascend-cann-kernels-910_${CANN_VERSION}_linux-${ARCH}.run"

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

echo "=== Installing system dependencies (requires sudo) ==="
if sudo -n true 2>/dev/null; then
  sudo apt-get update -qq
  sudo apt-get install -y \
    gcc g++ make cmake python3 python3-dev python3-pip \
    zlib1g zlib1g-dev openssl libsqlite3-dev libssl-dev libffi-dev \
    unzip pciutils net-tools libblas-dev gfortran
else
  echo "[skip] sudo not available; ensure gcc/cmake/python3 are installed"
  if ! python3 -m pip --version &>/dev/null; then
    curl -sS https://bootstrap.pypa.io/get-pip.py -o /tmp/get-pip.py
    python3 /tmp/get-pip.py --user
    export PATH="${HOME}/.local/bin:${PATH}"
  fi
fi

echo "=== Installing Python packages ==="
python3 -m pip install --user \
  attrs cython 'numpy>=1.19.2,<=1.24.0' decorator sympy cffi pyyaml \
  pathlib2 psutil 'protobuf==3.20.0' scipy requests absl-py

echo "=== Downloading CANN packages ==="
download_if_missing "${TOOLKIT_RUN}" "${REPO_BASE}/CANN/${ARCH}/${TOOLKIT_RUN}"
download_if_missing "${KERNELS_RUN}" "${REPO_BASE}/Ascend910/${ARCH}/${KERNELS_RUN}"

echo "=== Installing Toolkit ==="
chmod +x "${TOOLKIT_RUN}"
./"${TOOLKIT_RUN}" --install

echo "=== Installing Kernels (910) ==="
chmod +x "${KERNELS_RUN}"
./"${KERNELS_RUN}" --install

echo "=== Configuring environment ==="
SET_ENV="${HOME}/Ascend/ascend-toolkit/set_env.sh"
if [[ ! -f "${SET_ENV}" ]]; then
  echo "ERROR: ${SET_ENV} not found after install"
  exit 1
fi

MARKER="# Ascend CANN ${CANN_VERSION}"
BASHRC="${HOME}/.bashrc"
if ! grep -qF "${MARKER}" "${BASHRC}" 2>/dev/null; then
  cat >> "${BASHRC}" <<EOF

${MARKER}
source ${SET_ENV}
export LD_LIBRARY_PATH=\${HOME}/Ascend/ascend-toolkit/latest/${ARCH}-linux/devlib/:\${LD_LIBRARY_PATH}
EOF
fi

# shellcheck source=/dev/null
source "${SET_ENV}"
export LD_LIBRARY_PATH="${HOME}/Ascend/ascend-toolkit/latest/${ARCH}-linux/devlib/:${LD_LIBRARY_PATH:-}"

echo "=== Verifying installation ==="
INFO="${HOME}/Ascend/ascend-toolkit/latest/${ARCH}-linux/ascend_toolkit_install.info"
if [[ -f "${INFO}" ]]; then
  grep -E 'version|install_path' "${INFO}" || cat "${INFO}"
else
  ls -la "${HOME}/Ascend/ascend-toolkit/latest/" || true
fi

which ascendc 2>/dev/null || true
echo "CANN install complete."
