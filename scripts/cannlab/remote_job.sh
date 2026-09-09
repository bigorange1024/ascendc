#!/usr/bin/env bash
# remote_job.sh — 通用：远端 nohup 跑任务 + 心跳；本机只短连 submit/poll/fetch
#
# 解决：subagent 长 SSH 空闲 → SOCKS/Tailscale 断；或 30min 无 heartbeat → watchdog 停机。
#
# 用法：
#   SSH_HOST=cannlab-npu-1 bash scripts/cannlab/remote_job.sh submit --name myjob -- payload.sh
#   bash scripts/cannlab/remote_job.sh submit --name myjob -- bash -lc 'cd ...; bash run.sh ...'
#   bash scripts/cannlab/remote_job.sh poll
#   bash scripts/cannlab/remote_job.sh status
#   bash scripts/cannlab/remote_job.sh fetch
#   bash scripts/cannlab/remote_job.sh kill
#
# 环境：SSH_HOST / SOCKS / SSH_KEY；STATE_FILE 记最近 job_dir
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib_ssh.sh
source "${SCRIPT_DIR}/lib_ssh.sh"

STATE_FILE="${STATE_FILE:-/tmp/cannlab_remote_job.txt}"
ART="${ART:-/opt/cursor/artifacts}"
HB_INTERVAL="${HB_INTERVAL:-45}"

usage() {
  cat >&2 <<'U'
Usage:
  remote_job.sh submit [--name NAME] [--] SCRIPT_OR_CMD...
  remote_job.sh poll|status|fetch|kill
U
  exit 1
}

cmd="${1:-}"
shift || true

case "${cmd}" in
submit)
  name="job"
  while [ $# -gt 0 ]; do
    case "$1" in
      --name) name="$2"; shift 2 ;;
      --) shift; break ;;
      -*) usage ;;
      *) break ;;
    esac
  done
  [ $# -ge 1 ] || usage

  cannlab_pick_host
  job_id="${name}_$(date +%Y%m%d_%H%M%S)"
  job_dir="/mnt/workspace/jobs/${job_id}"
  echo "${job_dir}" >"${STATE_FILE}"
  echo "${SSH_HOST}" >>"${STATE_FILE}"

  payload_local="$(mktemp)"
  if [ $# -eq 1 ] && [ -f "$1" ]; then
    cat "$1" >"${payload_local}"
  else
    {
      echo '#!/usr/bin/env bash'
      echo 'set -uo pipefail'
      printf ' %q' "$@"
      echo
    } >"${payload_local}"
  fi

  wrapper_local="$(mktemp)"
  cat >"${wrapper_local}" <<WR
#!/usr/bin/env bash
# 远端包装：心跳 + 用户 payload；SSH 断开不影响本进程
set -uo pipefail
JOB_DIR='${job_dir}'
HB=/mnt/workspace/.agent_heartbeat
REPORT="\${JOB_DIR}/REPORT.txt"
echo RUNNING >"\${JOB_DIR}/STATUS"
: >"\${REPORT}"
while true; do touch "\${HB}"; sleep ${HB_INTERVAL}; done &
echo \$! >"\${JOB_DIR}/heartbeat.pid"
echo "[remote_job] start \$(date -Iseconds)" | tee -a "\${REPORT}"
set +e
bash "\${JOB_DIR}/payload.sh" >>"\${JOB_DIR}/payload.log" 2>&1
rc=\$?
set -e
echo "[remote_job] payload exit=\${rc} \$(date -Iseconds)" | tee -a "\${REPORT}"
kill "\$(cat "\${JOB_DIR}/heartbeat.pid")" 2>/dev/null || true
if [ "\${rc}" -eq 0 ]; then
  echo DONE >"\${JOB_DIR}/STATUS"
else
  echo FAIL >"\${JOB_DIR}/STATUS"
  echo "rc=\${rc}" >>"\${JOB_DIR}/STATUS"
fi
touch "\${HB}"
WR

  cannlab_ssh_try "mkdir -p '${job_dir}' && touch /mnt/workspace/.agent_heartbeat"
  scp_opts=()
  cannlab_scp_opts scp_opts
  scp "${scp_opts[@]}" "${payload_local}" "developer@${SSH_HOST}:${job_dir}/payload.sh"
  scp "${scp_opts[@]}" "${wrapper_local}" "developer@${SSH_HOST}:${job_dir}/wrapper.sh"
  rm -f "${payload_local}" "${wrapper_local}"

  cannlab_ssh_try "bash -s" <<EOF
set -euo pipefail
chmod +x '${job_dir}/payload.sh' '${job_dir}/wrapper.sh'
nohup bash '${job_dir}/wrapper.sh' >'${job_dir}/wrapper.log' 2>&1 &
echo \$! >'${job_dir}/job.pid'
echo JOB_DIR=${job_dir}
echo PID=\$(cat '${job_dir}/job.pid')
touch /mnt/workspace/.agent_heartbeat
EOF
  echo "[remote_job] submitted ${job_dir} on ${SSH_HOST} (state ${STATE_FILE})"
  ;;

poll|status)
  job_dir="$(sed -n '1p' "${STATE_FILE}" 2>/dev/null || true)"
  saved_host="$(sed -n '2p' "${STATE_FILE}" 2>/dev/null || true)"
  job_dir="${JOB_DIR:-${job_dir}}"
  [ -n "${job_dir}" ] || { echo "need STATE_FILE or JOB_DIR"; exit 1; }
  [ -n "${saved_host}" ] && SSH_HOST="${saved_host}"
  cannlab_pick_host
  if [ "${cmd}" = status ]; then
    cannlab_ssh_try "touch /mnt/workspace/.agent_heartbeat; echo STATUS=\$(cat '${job_dir}/STATUS' 2>/dev/null); tail -12 '${job_dir}/REPORT.txt' 2>/dev/null; tail -5 '${job_dir}/payload.log' 2>/dev/null; pgrep -af 'wrapper.sh|payload|ascendc_kem|run.sh' | head -6 || true"
    exit 0
  fi
  echo "[remote_job] polling ${job_dir} @ ${SSH_HOST} (Ctrl-C ok; remote continues)"
  while true; do
    if out="$(cannlab_ssh_try "touch /mnt/workspace/.agent_heartbeat; echo STATUS=\$(cat '${job_dir}/STATUS' 2>/dev/null); tail -8 '${job_dir}/REPORT.txt' 2>/dev/null; pgrep -af 'wrapper.sh|payload|ascendc_kem' | head -4 || true")"; then
      echo "[$(date +%H:%M:%S)]"
      echo "${out}"
      if echo "${out}" | grep -q 'STATUS=DONE'; then
        echo "[remote_job] DONE"; exit 0
      fi
      if echo "${out}" | grep -q 'STATUS=FAIL'; then
        echo "[remote_job] FAIL"; exit 1
      fi
    else
      echo "[$(date +%H:%M:%S)] SSH down — retry (remote job should continue if node alive)"
    fi
    sleep "${HB_INTERVAL}"
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
  scp "${scp_opts[@]}" "developer@${SSH_HOST}:${job_dir}/"* "${dest}/" || true
  echo "[remote_job] → ${dest}"
  ls -la "${dest}" || true
  ;;

kill)
  job_dir="$(sed -n '1p' "${STATE_FILE}" 2>/dev/null || true)"
  saved_host="$(sed -n '2p' "${STATE_FILE}" 2>/dev/null || true)"
  job_dir="${JOB_DIR:-${job_dir}}"
  [ -n "${saved_host}" ] && SSH_HOST="${saved_host}"
  cannlab_pick_host
  cannlab_ssh_try "bash -s" <<EOF
set +e
pid=\$(cat '${job_dir}/job.pid' 2>/dev/null)
hbp=\$(cat '${job_dir}/heartbeat.pid' 2>/dev/null)
kill "\${pid}" "\${hbp}" 2>/dev/null
pkill -f '${job_dir}/' 2>/dev/null
echo KILLED >'${job_dir}/STATUS'
touch /mnt/workspace/.agent_heartbeat
EOF
  ;;

*)
  usage
  ;;
esac
