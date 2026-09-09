#!/usr/bin/env bash
# live_npu_progress.sh — 每 10s 从云端日志刷真实进度到 NPU_LIVE（必须有 ok/fail/iter，禁止空「最近」）
set -uo pipefail
export TS_SOCK="${TS_SOCK:-$HOME/.local/share/tailscale/tailscaled.sock}"
REPO=/home/yuanye/ascendc
LIVE="${REPO}/graph-tests/decrypt-rebuild-ops/NPU_LIVE.md"
cd "${REPO}"
source scripts/cannlab/lib_ssh.sh

while true; do
  if ! cannlab_pick_host; then
    printf '%s\n' "# NPU LIVE" "> $(date -Iseconds)" "| 状态 | host_offline |" > "$LIVE"
    sleep 10; continue
  fi
  cannlab_ssh_base sshc
  # SSH 失败时保留上一帧好数据，并标明 stale
  if ! snap=$("${sshc[@]}" 'echo TS=$(date -Iseconds)
    for tag in d01 d02 d03 k01 k02 k03 d04; do
      f=/tmp/rb-${tag}-x30-npu.log
      if [ -f "$f" ]; then
        last=$(grep -E "OK |ALL_DONE|ABORT|FAIL|START|error" "$f" | tail -1 | tr -d "\r")
        n=$(grep -cE "_OK " "$f" 2>/dev/null || echo 0)
        echo "LINE ${tag}|${n}|${last}"
      fi
    done
    if pgrep -f "/tmp/d03_x30_npu_job" >/dev/null; then echo RUN=d03
    elif pgrep -f "/tmp/d02_x30_npu_job" >/dev/null; then echo RUN=d02
    elif pgrep -f "/tmp/d01_x30_npu_job" >/dev/null; then echo RUN=d01
    elif pgrep -af ascendc_kernels_bbit | grep -v pgrep >/dev/null; then echo RUN=kernel
    else echo RUN=idle
    fi
  ' 2>/tmp/live_ssh.err); then
    err=$(head -c 160 /tmp/live_ssh.err | tr '\n' ' ')
    if [[ -f /tmp/live_npu_last_good.txt ]]; then
      snap=$(cat /tmp/live_npu_last_good.txt)
      snap="${snap}"$'\n'"STALE=ssh_fail ${err}"
    else
      snap="SSH_FAIL ${err}"$'\n'"RUN=unknown"
    fi
  else
    printf '%s\n' "${snap}" > /tmp/live_npu_last_good.txt
  fi

  run=$(echo "$snap" | grep '^RUN=' | cut -d= -f2)
  # 优先展示当前 RUN 对应行，否则最新 LINE
  cur_line=$(echo "$snap" | grep "^LINE ${run}|" | tail -1)
  [[ -z "$cur_line" ]] && cur_line=$(echo "$snap" | grep '^LINE ' | tail -1)
  ok_n=$(echo "$cur_line" | cut -d'|' -f2)
  last=$(echo "$cur_line" | cut -d'|' -f3-)
  [[ -z "$last" ]] && last="(无日志行 — 作业可能未启动)"

  cat > "$LIVE" <<EOF
# NPU 实时状态（\`live_npu_progress\` 每 10s）

> 刷新：$(date -Iseconds)

| 项 | 值 |
|----|-----|
| 进程 | **${run}** |
| OK计数 | ${ok_n:-—} |
| 最近日志 | \`${last}\` |
| 证据 | 云 \`/tmp/rb-*-x30-npu.log\` |

<details><summary>snap</summary>

\`\`\`
${snap}
\`\`\`
</details>
EOF
  sleep 10
done
