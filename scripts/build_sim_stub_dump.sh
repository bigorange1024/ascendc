#!/usr/bin/env bash
# Build empty libascend_dump.so into a case's out/lib for WSL SIM (avoids InitHardwareInfo950 FPE).
set -euo pipefail
dest="${1:?usage: build_sim_stub_dump.sh <case_dir>}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${dest}/out/lib"
gcc -shared -fPIC -o "${dest}/out/lib/libascend_dump.so" \
  "${repo_root}/scripts/stub_libascend_dump.c"
