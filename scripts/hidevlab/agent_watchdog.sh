#!/usr/bin/env bash
# HiDevLab 空闲看门狗：连续 IDLE_MIN 分钟无活动则尝试停容器，避免卡时白烧。
# 由 agent_bootstrap.sh 拉起。平台另有「连续 1h 未使用自动关机」——本狗为 Agent 侧补充。
#
# 活动判定（任一为真即清零）：
#   1) /workspace/.agent_heartbeat 在 IDLE_MIN 内被 touch（Agent SSH 会 touch）
#   2) 重负载：ccec / cmake --build / msprof / *_npu / gen_data.py / run.sh
#   3) npu-smi 成功且明确有设备进程（失败≠active；旧 bug 曾导致永不熄火）
# 已删除：交互 pts 空闲（WebIDE 挂着 idle shell 会被当成 forever-active）
#
# 默认不随 bootstrap 拉起（须 ENABLE_WATCHDOG=1）。停机：SIGTERM→PID1；
# 计费以 HiDevLab 控制台关机为准——看门狗失败时勿依赖它。
set -u
IDLE_MIN="${IDLE_MIN:-45}"
HB="${HIDEVLAB_HEARTBEAT:-/workspace/.agent_heartbeat}"
LOG="${HIDEVLAB_WATCHDOG_LOG:-/workspace/agent_watchdog.log}"
DRV=/usr/local/Ascend/driver/lib64
export LD_LIBRARY_PATH="${DRV}:${DRV}/driver:${DRV}/common:${LD_LIBRARY_PATH:-}"
idle=0
echo "[$(date '+%F %T')] hidevlab watchdog start IDLE_MIN=${IDLE_MIN} pid=$$" >>"${LOG}"
while true; do
  active=0
  if [ -f "${HB}" ]; then
    age=$(( $(date +%s) - $(stat -c %Y "${HB}" 2>/dev/null || echo 0) ))
    [ "${age}" -lt $((IDLE_MIN*60)) ] && active=1
  fi
  if pgrep -f -a 'ccec|cmake --build|msprof|/[A-Za-z0-9_]*_npu( |$)|gen_data\.py|run\.sh' 2>/dev/null \
    | grep -vq agent_watchdog; then
    active=1
  fi
  # 仅当 npu-smi 成功且明确看到非空进程表时才算 active。
  # 旧逻辑：npu-smi 失败（缺 libc_sec / 未设 LD）→ 误判 active=1 → 永远不清闲 → 空转白烧。
  if command -v npu-smi >/dev/null 2>&1; then
    if npu-smi info >/tmp/.wd_npu_hidev 2>/dev/null; then
      if grep -qE 'Process id|Process name' /tmp/.wd_npu_hidev \
        && ! grep -qE 'No running processes|No Process' /tmp/.wd_npu_hidev; then
        active=1
      fi
    fi
  fi
  # 不再用「交互 pts 空闲」判定：WebIDE 常挂着一个几乎不动的 shell，
  # 会被当成 forever-active，看门狗永不触发（本次空转 ~10h 的主因之一）。
  if [ "${active}" = "1" ]; then idle=0; else idle=$((idle+1)); fi
  echo "[$(date '+%F %T')] active=${active} idle=${idle}/${IDLE_MIN}min" >>"${LOG}"
  if [ "${idle}" -ge "${IDLE_MIN}" ]; then
    echo "[$(date '+%F %T')] IDLE ${IDLE_MIN}min → stop (SIGTERM→PID1)" >>"${LOG}"
    sync
    kill -TERM 1 2>/dev/null || true
    sleep 5
    poweroff 2>/dev/null || halt -f 2>/dev/null || true
    echo "[$(date '+%F %T')] 已尝试停机；计费以 HiDevLab 控制台为准" >>"${LOG}"
    exit 0
  fi
  sleep 60
done
