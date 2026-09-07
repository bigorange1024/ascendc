#!/usr/bin/env bash
# hang_observe_encaps_decaps.sh — CANNLab 上亲自多轮/交叉跑 stable Encaps/Decaps 取粘性挂样貌
#
# 前置：本机已按 docs/engineering/CANNLab接入与远程驱动.md §4 接入 Tailscale + SSH。
# 用法：
#   bash scripts/cannlab/hang_observe_encaps_decaps.sh
# 可选：
#   ROUNDS=5 SOC=Ascend910B3 BRANCH=cursor/kem-2launch-sticky-1534 \
#     bash scripts/cannlab/hang_observe_encaps_decaps.sh
#
# 只观察挂/超时；不做正确性结案。跑完请控制台关机。
set -euo pipefail

ROUNDS="${ROUNDS:-7}"
SOC="${SOC:-Ascend910B3}"
BRANCH="${BRANCH:-cursor/kem-2launch-sticky-1534}"
TIMEOUT_SEC="${TIMEOUT_SEC:-900}"
SSH_KEY="${SSH_KEY:-$HOME/.ssh/cannlab}"

SSH=(ssh
  -o ServerAliveInterval=15
  -o ProxyCommand="nc -X 5 -x 127.0.0.1:1055 %h %p"
  -o StrictHostKeyChecking=accept-new
  -i "${SSH_KEY}"
  -p 2222
  developer@cannlab-npu
)

echo "[hang-observe] connect check…"
"${SSH[@]}" 'echo CONNECTED pid1=$(cat /proc/1/comm); hostname; npu-smi info -l 2>/dev/null | head -20 || true'

echo "[hang-observe] remote Encaps×${ROUNDS} + Decaps×${ROUNDS} + cross on ${BRANCH} (${SOC})"
"${SSH[@]}" "bash -s" <<R
set -euo pipefail
export LD_LIBRARY_PATH=/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:\${LD_LIBRARY_PATH:-}
source /home/developer/Ascend/ascend-toolkit/set_env.sh
cd /mnt/workspace/ascendc
git fetch origin
git checkout ${BRANCH}
git pull --ff-only origin ${BRANCH} || git pull --ff-only
export ASCEND_DEVICE_ID=0 CANNLAB=1 CMAKE_BUILD_JOBS=\${CMAKE_BUILD_JOBS:-8}
export KEM_ENCAPS_FORCE_REBUILD=1 KEM_DECAPS_FORCE_REBUILD=1
# 首轮强制重建后可跳过重编以贴近「多轮粘性」墙钟（仍可 FORCE）
ROOT=examples/stable/ml-kem/ml-kem-1024
ENC=\$ROOT/stable-fips203-mlkem-kem-encaps-k4
DEC=\$ROOT/stable-fips203-mlkem-kem-decaps-k4

run_one() {
  local label="\$1" dir="\$2" round="\$3"
  echo "===== \${label} round \${round} ====="
  set +e
  timeout ${TIMEOUT_SEC} bash -lc "cd '\${dir}' && bash run.sh -r npu -v ${SOC}"
  local rc=\$?
  set -e
  if [ "\$rc" -eq 124 ]; then
    echo "REPORT: \${label} HANG TIMEOUT124 round=\${round}"
    return 124
  elif [ "\$rc" -ne 0 ]; then
    echo "REPORT: \${label} FAIL rc=\${rc} round=\${round}"
    return "\$rc"
  fi
  echo "REPORT: \${label} PASS round=\${round}"
  # 后续轮次允许 SKIP，贴近历史「旧二进制多轮」；需要每轮重编则保持 FORCE=1
  export KEM_ENCAPS_FORCE_REBUILD=0 KEM_DECAPS_FORCE_REBUILD=0
  return 0
}

for i in \$(seq 1 ${ROUNDS}); do
  run_one ENCAPS "\$ENC" "\$i" || break
done
for i in \$(seq 1 ${ROUNDS}); do
  run_one DECAPS "\$DEC" "\$i" || break
done
echo "===== CROSS encaps then decaps ====="
run_one ENCAPS "\$ENC" cross1 || true
run_one DECAPS "\$DEC" cross1 || true
echo "===== HANG_OBSERVE_DONE ====="
R

echo "[hang-observe] done — 请到 CANNLab 控制台关机/停止确认停计费"
