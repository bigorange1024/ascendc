#!/usr/bin/env bash
# CANNLab 空闲看门狗：连续 IDLE_MIN 分钟无“活动”即停止容器，避免 NPU 卡时白烧。
# 本文件为仓库内**权威版**；CANNLab 上一般由 agent_bootstrap.sh 拉起（同目录）。
#
# 活动判定（任一为真即视为活跃、清零计时）：
#   1) 心跳文件 /mnt/workspace/.agent_heartbeat 在 IDLE_MIN 内被 touch（Agent 每次远程命令会 touch）
#   2) 有重负载进程在跑：ccec / cmake --build / msprof / *_npu / gen_data.py / run.sh
#   3) NPU 上有 running process（npu-smi）
#   4) 有交互 pts 会话且 IDLE < IDLE_MIN（保护正在 WebIDE 里操作的人）
#
# 停止方式（重要）：CANNLab 是 Docker 容器（PID1=tini，无 systemd）。
#   poweroff / halt / shutdown 均**无效**；SIGKILL 到 PID1 被内核拦截。
#   实测 **SIGTERM 到 tini(PID1)** 能让容器优雅退出、tailnet 节点下线。
#   注意：容器下线**不一定**等于 GitCode 停止计费 —— 权威停止以**控制台“关机/停止”**为准。
set -u
IDLE_MIN="${IDLE_MIN:-30}"
HB=/mnt/workspace/.agent_heartbeat
LOG=/mnt/workspace/agent_watchdog.log
DRV=/usr/local/Ascend/driver/lib64
export LD_LIBRARY_PATH="${DRV}:${DRV}/driver:${DRV}/common:${LD_LIBRARY_PATH:-}"
idle=0
echo "[$(date '+%F %T')] watchdog start IDLE_MIN=${IDLE_MIN} pid=$$" >>"${LOG}"
while true; do
  active=0
  # 1) 心跳
  if [ -f "${HB}" ]; then
    age=$(( $(date +%s) - $(stat -c %Y "${HB}" 2>/dev/null || echo 0) ))
    [ "${age}" -lt $((IDLE_MIN*60)) ] && active=1
  fi
  # 2) 重负载进程（排除看门狗自身）
  pgrep -f -a 'ccec|cmake --build|msprof|/[A-Za-z0-9_]*_npu( |$)|gen_data\.py|run\.sh' 2>/dev/null \
    | grep -vq agent_watchdog && active=1
  # 3) NPU running process
  if command -v npu-smi >/dev/null 2>&1; then
    npu-smi info >/tmp/.wd_npu 2>/dev/null && grep -q 'No running processes' /tmp/.wd_npu || active=1
  fi
  # 4) 交互会话（utmp 有记录时才生效）
  if command -v w >/dev/null 2>&1; then
    while read -r i; do
      case "${i}" in
        *s) m=${i%s}; [ "${m:-999}" -lt $((IDLE_MIN*60)) ] && active=1 ;;
        *m) m=${i%m}; [ "${m:-999}" -lt "${IDLE_MIN}" ] && active=1 ;;
        *) : ;;
      esac
    done < <(w -h -s 2>/dev/null | awk '{print $4}')
  fi
  if [ "${active}" = "1" ]; then idle=0; else idle=$((idle+1)); fi
  echo "[$(date '+%F %T')] active=${active} idle=${idle}/${IDLE_MIN}min" >>"${LOG}"
  if [ "${idle}" -ge "${IDLE_MIN}" ]; then
    echo "[$(date '+%F %T')] IDLE ${IDLE_MIN}min → stop container (SIGTERM→tini)" >>"${LOG}"
    sync
    sudo kill -TERM 1 2>/dev/null
    sleep 5
    sudo poweroff 2>/dev/null; sudo halt -f 2>/dev/null   # 容器里多半无效，兜底尝试
    echo "[$(date '+%F %T')] 已尝试 SIGTERM→tini；若计费未停请到 GitCode 控制台点“关机/停止”" >>"${LOG}"
    exit 0
  fi
  sleep 60
done
