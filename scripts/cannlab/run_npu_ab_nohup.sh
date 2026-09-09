#!/usr/bin/env bash
# run_npu_ab_nohup.sh — Encaps NPU-A/B：远端 nohup + 心跳；本机短连 submit/poll/fetch
#
# 背景：subagent 长 SSH 空闲 → SOCKS/Tailscale 断，或 30min 无 touch 心跳被 watchdog 停机。
# 对策：实验整段在 CANNLab 后台跑，并每 45s touch /mnt/workspace/.agent_heartbeat。
#
# 本机（Tailscale userspace + SOCKS 1055 + ~/.ssh/cannlab）：
#   bash scripts/cannlab/which_npu.sh          # 看当前 online 主机名（每次开机可能变）
#   bash scripts/cannlab/run_npu_ab_nohup.sh submit
#   bash scripts/cannlab/run_npu_ab_nohup.sh poll
#   bash scripts/cannlab/run_npu_ab_nohup.sh fetch
#   nohup bash scripts/cannlab/agent_link_keepalive.sh &   # 建议
# 勿写死 SSH_HOST=cannlab-npu-1；仅排障用 SSH_HOST_FORCE=...
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib_ssh.sh
source "${SCRIPT_DIR}/lib_ssh.sh"

ROUNDS="${ROUNDS:-12}"
TIMEOUT_SEC="${TIMEOUT_SEC:-240}"
# 已知粘性挂时：默认首挂即停，禁止空等满轮烧卡时（用户 2026-09-08）
# 仅当明确要挂率统计时：STOP_ON_HANG=0 MAX_HANG=999
STOP_ON_HANG="${STOP_ON_HANG:-1}"
MAX_HANG="${MAX_HANG:-1}"
SOC="${SOC:-Ascend910B3}"
STATE_FILE="${STATE_FILE:-/tmp/npu_ab_job_dir.txt}"
ART="${ART:-/opt/cursor/artifacts}"

ssh_try() { cannlab_ssh_try "$@"; }

cmd="${1:-}"

case "${cmd}" in
submit)
  cannlab_pick_host
  job_id="npu_ab_$(date +%Y%m%d_%H%M%S)"
  job_dir="/mnt/workspace/jobs/${job_id}"
  {
    echo "${job_dir}"
    echo "${SSH_HOST}"
  } >"${STATE_FILE}"
  payload_local="$(mktemp)"
  cat >"${payload_local}" <<'PL'
#!/usr/bin/env bash
# Encaps NPU-A (no TRACE) → NPU-B (F203_L18_TRACE=1)
# 默认 STOP_ON_HANG：任一 phase 累计 HANG 达 MAX_HANG 即整 job 结束（禁空等满轮）
set -uo pipefail
: "${JOB_DIR:?}" "${ROUNDS:?}" "${TIMEOUT_SEC:?}" "${SOC:?}"
STOP_ON_HANG="${STOP_ON_HANG:-1}"
MAX_HANG="${MAX_HANG:-1}"
ENC=/mnt/workspace/ascendc/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4
REPORT="${JOB_DIR}/REPORT.txt"
: >"${REPORT}"
echo RUNNING >"${JOB_DIR}/STATUS"

while true; do touch /mnt/workspace/.agent_heartbeat; sleep 45; done &
echo $! >"${JOB_DIR}/heartbeat.pid"

export LD_LIBRARY_PATH=/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:${LD_LIBRARY_PATH:-}
set +u
# shellcheck disable=SC1091
source /home/developer/Ascend/ascend-toolkit/set_env.sh 2>/dev/null || true
set -u
export ASCEND_DEVICE_ID=0 CANNLAB=1 CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-8}"

run_phase() {
  local phase="$1"
  local with_trace="$2" # 0|1
  local hangs=0
  echo "===== PHASE ${phase} start $(date -Iseconds) STOP_ON_HANG=${STOP_ON_HANG} MAX_HANG=${MAX_HANG} =====" | tee -a "${REPORT}"
  export KEM_ENCAPS_FORCE_REBUILD=1
  local i rc
  for i in $(seq 1 "${ROUNDS}"); do
    touch /mnt/workspace/.agent_heartbeat
    echo "----- ${phase} round ${i} -----" | tee -a "${REPORT}"
    set +e
    if [ "${with_trace}" = "1" ]; then
      timeout "${TIMEOUT_SEC}" bash -lc \
        "cd '${ENC}' && export F203_L18_TRACE=1 ASCEND_DEVICE_ID=0 CANNLAB=1 && bash run.sh -r npu -v ${SOC}" \
        >>"${JOB_DIR}/phase_${phase}.log" 2>&1
    else
      timeout "${TIMEOUT_SEC}" bash -lc \
        "cd '${ENC}' && unset F203_L18_TRACE && export ASCEND_DEVICE_ID=0 CANNLAB=1 && bash run.sh -r npu -v ${SOC}" \
        >>"${JOB_DIR}/phase_${phase}.log" 2>&1
    fi
    rc=$?
    set -e
    if [ "${rc}" -eq 124 ]; then
      hangs=$((hangs + 1))
      echo "REPORT: ${phase} HANG TIMEOUT124 round=${i} hangs=${hangs}" | tee -a "${REPORT}"
      grep -E 'l18-trace|SynchronizeExecutedTask|launch 2|SUCCESS|FAIL|verify' \
        "${JOB_DIR}/phase_${phase}.log" | tail -40 >>"${REPORT}" || true
      if [ "${STOP_ON_HANG}" = "1" ] && [ "${hangs}" -ge "${MAX_HANG}" ]; then
        echo "REPORT: ${phase} STOP_ON_HANG after round=${i}" | tee -a "${REPORT}"
        echo "===== PHASE ${phase} aborted $(date -Iseconds) =====" | tee -a "${REPORT}"
        return 2
      fi
    elif [ "${rc}" -eq 0 ]; then
      echo "REPORT: ${phase} PASS round=${i}" | tee -a "${REPORT}"
    else
      echo "REPORT: ${phase} FAIL rc=${rc} round=${i}" | tee -a "${REPORT}"
      grep -E 'l18-trace|verify|FAIL|SUCCESS' "${JOB_DIR}/phase_${phase}.log" | tail -25 >>"${REPORT}" || true
    fi
    export KEM_ENCAPS_FORCE_REBUILD=0
  done
  echo "===== PHASE ${phase} done $(date -Iseconds) =====" | tee -a "${REPORT}"
  return 0
}

run_phase A 0
rcA=$?
if [ "${rcA}" -eq 2 ] && [ "${STOP_ON_HANG}" = "1" ]; then
  echo "REPORT: skip B — A already hit hang stop" | tee -a "${REPORT}"
else
  run_phase B 1 || true
fi

kill "$(cat "${JOB_DIR}/heartbeat.pid")" 2>/dev/null || true
echo DONE >"${JOB_DIR}/STATUS"
echo "[remote-ab] finished $(date -Iseconds)" | tee -a "${REPORT}"
PL

  ssh_try "mkdir -p '${job_dir}' && touch /mnt/workspace/.agent_heartbeat"
  scp_opts=()
  cannlab_scp_opts scp_opts
  scp "${scp_opts[@]}" \
    "${payload_local}" "developer@${SSH_HOST}:${job_dir}/payload.sh"
  rm -f "${payload_local}"

  ssh_try "bash -s" <<EOF
set -euo pipefail
chmod +x '${job_dir}/payload.sh'
{ echo "export JOB_DIR='${job_dir}'"; echo "export ROUNDS='${ROUNDS}'"; echo "export TIMEOUT_SEC='${TIMEOUT_SEC}'"; echo "export SOC='${SOC}'"; echo "export STOP_ON_HANG='${STOP_ON_HANG}'"; echo "export MAX_HANG='${MAX_HANG}'"; cat '${job_dir}/payload.sh'; } >'${job_dir}/payload.run.sh'
chmod +x '${job_dir}/payload.run.sh'
echo RUNNING >'${job_dir}/STATUS'
nohup bash '${job_dir}/payload.run.sh' >'${job_dir}/wrapper.log' 2>&1 &
echo \$! >'${job_dir}/job.pid'
echo JOB_DIR=${job_dir}
echo PID=\$(cat '${job_dir}/job.pid')
touch /mnt/workspace/.agent_heartbeat
EOF
  echo "[remote-ab] submitted ${job_dir} on ${SSH_HOST} STOP_ON_HANG=${STOP_ON_HANG} MAX_HANG=${MAX_HANG}"
  ;;

poll)
  job_dir="$(sed -n '1p' "${STATE_FILE}" 2>/dev/null || true)"
  saved_host="$(sed -n '2p' "${STATE_FILE}" 2>/dev/null || true)"
  job_dir="${JOB_DIR:-${job_dir}}"
  [ -n "${job_dir}" ] || { echo "need STATE_FILE or JOB_DIR"; exit 1; }
  [ -n "${saved_host}" ] && SSH_HOST="${saved_host}"
  cannlab_pick_host
  echo "[remote-ab] polling ${job_dir} @ ${SSH_HOST} (Ctrl-C ok; job keeps running on NPU)"
  while true; do
    if out="$(ssh_try "touch /mnt/workspace/.agent_heartbeat; echo STATUS=\$(cat '${job_dir}/STATUS' 2>/dev/null); tail -8 '${job_dir}/REPORT.txt' 2>/dev/null; pgrep -af 'payload.run|ascendc_kem' | head -4 || true")"; then
      echo "[$(date +%H:%M:%S)]"
      echo "${out}"
      if echo "${out}" | grep -q 'STATUS=DONE'; then
        echo "[remote-ab] DONE"
        exit 0
      fi
      if echo "${out}" | grep -q 'STATUS=FAIL'; then
        echo "[remote-ab] FAIL"
        exit 1
      fi
    else
      echo "[$(date +%H:%M:%S)] SSH down — will retry (remote job should continue if node alive)"
    fi
    sleep 45
  done
  ;;

fetch)
  job_dir="$(sed -n '1p' "${STATE_FILE}" 2>/dev/null || true)"
  saved_host="$(sed -n '2p' "${STATE_FILE}" 2>/dev/null || true)"
  job_dir="${JOB_DIR:-${job_dir}}"
  [ -n "${job_dir}" ] || { echo "need JOB_DIR"; exit 1; }
  [ -n "${saved_host}" ] && SSH_HOST="${saved_host}"
  cannlab_pick_host
  base="$(basename "${job_dir}")"
  dest="${ART}/${base}"
  mkdir -p "${dest}"
  scp_opts=()
  cannlab_scp_opts scp_opts
  scp "${scp_opts[@]}" \
    "developer@${SSH_HOST}:${job_dir}/REPORT.txt" \
    "developer@${SSH_HOST}:${job_dir}/STATUS" \
    "developer@${SSH_HOST}:${job_dir}/phase_A.log" \
    "developer@${SSH_HOST}:${job_dir}/phase_B.log" \
    "developer@${SSH_HOST}:${job_dir}/wrapper.log" \
    "${dest}/" || true
  echo "[remote-ab] → ${dest}"
  ls -la "${dest}" || true
  ;;

keepalive)
  exec bash "${SCRIPT_DIR}/agent_link_keepalive.sh"
  ;;

*)
  echo "Usage: $0 submit|poll|fetch|keepalive" >&2
  exit 1
  ;;
esac
