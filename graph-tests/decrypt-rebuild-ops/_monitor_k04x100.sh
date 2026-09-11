#!/usr/bin/env bash
set -uo pipefail
export TS_SOCK=/home/yuanye/.local/share/tailscale/tailscaled.sock
LIVE=/home/yuanye/ascendc/graph-tests/decrypt-rebuild-ops/NPU_LIVE.md
cd /home/yuanye/ascendc
source scripts/cannlab/lib_ssh.sh
for n in $(seq 1 500); do
  cannlab_pick_host || { sleep 30; continue; }
  cannlab_ssh_base sshc
  summary=$("${sshc[@]}" 'grep -E "ITER_OK|ITER_FAIL|ALL_DONE|ABORT" /tmp/rb-k04-x100-npu.log | tail -3; pgrep -f "/tmp/k04_x100_npu_job" >/dev/null && echo ALIVE || echo DEAD')
  ts=$(date -Iseconds)
  last=$(echo "$summary" | grep -E "ITER_|ALL_DONE|ABORT" | tail -1)
  alive=$(echo "$summary" | grep -E "ALIVE|DEAD" | tail -1)
  cat > "$LIVE" <<EOF
# NPU 实时状态

> 刷新：$ts

| 项 | 值 |
|----|-----|
| 节点 | cannlab-npu-1 |
| 作业 | K04×100 |
| 最近 | $last |
| 进程 | $alive |

\`\`\`
$summary
\`\`\`
EOF
  echo "$summary" | grep -q ALL_DONE_K04_X100 && exit 0
  echo "$summary" | grep -q ABORT && exit 1
  if echo "$summary" | grep -q DEAD && ! echo "$summary" | grep -q ALL_DONE; then exit 2; fi
  sleep 40
done
