#!/usr/bin/env bash
# 自动化录屏（开发自测）；正常使用请手动打开 ?cinema=1 并点「开始演示」
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
WHICH="${3:-correctness}"
case "$WHICH" in
  bounded) PAGE="bounded-encaps-process-demo.html" ;;
  *) PAGE="correctness-encaps-process-demo.html" ;;
esac
URL="http://127.0.0.1:8765/${PAGE}?cinema=1&record=1"
OUT="${1:-/opt/cursor/artifacts/encaps_process_only.mp4}"
DUR="${2:-34}"
PROFILE="/tmp/chrome-encaps-rec-$$"
VIDDIR="/tmp/encaps-vid-$$"

if ! curl -sf -o /dev/null "${URL%%\?*}?cinema=1"; then
  echo "HTTP server not up. Start:" >&2
  echo "  python3 -m http.server 8765 --directory ${REPO_ROOT}/docs/reports/methodology-demo" >&2
  exit 1
fi

rm -rf "${PROFILE}" "${VIDDIR}"
mkdir -p "${PROFILE}/Default" "${VIDDIR}"
cat > "${PROFILE}/Default/Preferences" <<'PREF'
{"translate":{"enabled":false}}
PREF

google-chrome \
  --headless=new \
  --disable-gpu \
  --user-data-dir="${PROFILE}" \
  --record-video="${VIDDIR}" \
  --window-size=1280,720 \
  --no-first-run \
  --lang=zh-CN \
  --disable-features=Translate,TranslateUI \
  "${URL}" >/dev/null 2>&1 &
CPID=$!

cleanup() {
  kill "${CPID}" 2>/dev/null || true
  pkill -f "${PROFILE}" 2>/dev/null || true
  rm -rf "${PROFILE}"
}
trap cleanup EXIT

sleep "${DUR}"

WEBM="$(find "${VIDDIR}" -name '*.webm' | head -1)"
if [[ -z "${WEBM}" || ! -f "${WEBM}" ]]; then
  echo "FAIL: no webm in ${VIDDIR}" >&2
  ls -la "${VIDDIR}" >&2 || true
  exit 1
fi

ffmpeg -y -loglevel error -i "${WEBM}" -c:v libx264 -pix_fmt yuv420p -crf 20 "${OUT}"
rm -rf "${VIDDIR}"
echo "OK: ${OUT}"
