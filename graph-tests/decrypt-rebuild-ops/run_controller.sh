#!/usr/bin/env bash
# run_controller.sh — 单进程：真读日志刷 LIVE；缺刀且无日志才 launch
set -uo pipefail
export TS_SOCK="${TS_SOCK:-$HOME/.local/share/tailscale/tailscaled.sock}"
REPO=/home/yuanye/ascendc
LIVE="${REPO}/graph-tests/decrypt-rebuild-ops/NPU_LIVE.md"
INTERVAL=12
cd "${REPO}"
source scripts/cannlab/lib_ssh.sh

ensure_keepalive() {
  pgrep -f 'scripts/cannlab/agent_link_keepalive.sh' >/dev/null 2>&1 && return 0
  nohup bash "${REPO}/scripts/cannlab/agent_link_keepalive.sh" >>/tmp/cannlab_keepalive.log 2>&1 &
}

# 输出三行：d01|done|ALL_DONE...
probe() {
  cannlab_pick_host || { echo 'd01|sshfail|-'; echo 'd02|sshfail|-'; echo 'd03|sshfail|-'; return 1; }
  cannlab_ssh_base sshc
  "${sshc[@]}" 'for tag in d01 d02 d03; do
    TAG=$(echo "$tag" | tr a-z A-Z)
    log=/tmp/rb-${tag}-x30-npu.log
    if [ ! -f "$log" ]; then
      if pgrep -f /tmp/${tag}_x30_npu_job >/dev/null; then echo "${tag}|run|starting_no_log"
      else echo "${tag}|missing|no_log"; fi
      continue
    fi
    last=$(grep -E "${TAG}_OK|ALL_DONE|ABORT|START|error" "$log" | tail -1 | tr "|" "/" | tr "\n" " ")
    if grep -q "ALL_DONE_${TAG}_X30" "$log"; then echo "${tag}|done|${last}"
    elif pgrep -f /tmp/${tag}_x30_npu_job >/dev/null; then echo "${tag}|run|${last}"
    elif grep -q "${TAG}_OK" "$log"; then echo "${tag}|stuck|${last}"
    else echo "${tag}|stuck|${last:-empty}"; fi
  done'
}

launch() {
  local tag="$1"
  cannlab_scp_opts scpo
  [ -f /tmp/${tag}_x30_npu_job.sh ] || return 1
  scp -q "${scpo[@]}" /tmp/${tag}_x30_npu_job.sh "developer@${SSH_HOST}:/tmp/${tag}_x30_npu_job.sh" || return 1
  cannlab_ssh_base sshc
  "${sshc[@]}" "chmod +x /tmp/${tag}_x30_npu_job.sh
    pgrep -f /tmp/${tag}_x30_npu_job >/dev/null && echo ALREADY || nohup bash /tmp/${tag}_x30_npu_job.sh >/tmp/${tag}_x30_nohup.out 2>&1 &
    sleep 4
    test -s /tmp/rb-${tag}-x30-npu.log && echo LOG_OK=\$(wc -l </tmp/rb-${tag}-x30-npu.log) || echo LOG_MISSING"
}

echo "[controller] start $(date -Iseconds)" >>/tmp/run_controller.log

while true; do
  ensure_keepalive
  mapfile -t lines < <(probe 2>>/tmp/run_controller.log || true)
  declare -A st last
  for line in "${lines[@]:-}"; do
    [[ -z "$line" ]] && continue
    t=${line%%|*}; rest=${line#*|}; s=${rest%%|*}; l=${rest#*|}
    st[$t]=$s; last[$t]=$l
  done
  d1=${st[d01]:-sshfail}; d2=${st[d02]:-sshfail}; d3=${st[d03]:-sshfail}
  action=hold
  if [[ "$d1" == missing || "$d1" == stuck ]]; then action=launch_d01; launch d01 >>/tmp/run_controller.log 2>&1 || true
  elif [[ "$d1" == done && ( "$d2" == missing || "$d2" == stuck ) ]]; then action=launch_d02; launch d02 >>/tmp/run_controller.log 2>&1 || true
  elif [[ "$d2" == done && ( "$d3" == missing || "$d3" == stuck ) ]]; then action=launch_d03; launch d03 >>/tmp/run_controller.log 2>&1 || true
  elif [[ "$d1" == done && "$d2" == done && "$d3" == done ]]; then action=bricks_done
  elif [[ "$d1" == run || "$d2" == run || "$d3" == run ]]; then action=running
  fi

  cat > "$LIVE" <<EOF
# NPU 实时状态（run_controller · 真读日志）

> 刷新：$(date -Iseconds)

| 刀 | 状态 | 最近日志 |
|----|------|----------|
| D01×30 | **${d1}** | \`${last[d01]:-}\` |
| D02×30 | **${d2}** | \`${last[d02]:-}\` |
| D03×30 | **${d3}** | \`${last[d03]:-}\` |
| 动作 | ${action} | 缺 \`LOG_OK\` 不得宣称在跑 |

矩阵：见 \`MATRIX.md\`。砖级齐后做 DOC/CPU 冒烟，不空等。
EOF
  echo "$(date -Iseconds) $d1 $d2 $d3 $action" >>/tmp/run_controller.log
  sleep "$INTERVAL"
done
