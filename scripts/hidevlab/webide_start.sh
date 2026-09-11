#!/usr/bin/env bash
# HiDevLab WebIDE —— 每次开机只跑这一条（人侧）。
#
# 做什么：
#   1) （可选）配 CANN / LD_LIBRARY_PATH / ASCEND_DEVICE_ID（webide_boot.sh）
#   2) userspace Tailscale 入网 + 127.0.0.1:2222 sshd + serve（hidevlab_ts.sh）
#   3) 把本脚本与依赖拷到 /workspace/user_data/（持久盘），下次开机仍可跑
#
# 用法（WebIDE 终端）：
#   # 首次 / 仓库在 /workspace/ascendc 时：
#   TS_AUTHKEY='tskey-auth-…' bash /workspace/ascendc/scripts/hidevlab/webide_start.sh
#   # 之后推荐（持久盘，不依赖 git / overlay）：
#   TS_AUTHKEY='tskey-auth-…' bash /workspace/user_data/webide_start.sh
#
# 可选环境变量：
#   SKIP_BOOT=1          # 跳过 CANN 环境配置（只要 tailnet/sshd）
#   TS_HOSTNAME=…        # 默认 hidevlab-npu（勿用 cannlab-npu）
#   CURSOR_PUBKEY='…'    # 覆盖默认授权公钥
#   TS_AUTHKEY / TS_AUTHKEY  # 二者任一即可（兼容）
#
# 硬禁：apt 装 tailscale、内核态 TUN、tailscale up --ssh（会崩 WebIDE）。
# 与 CANNLab（scripts/cannlab/）分开维护。
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PERSIST_DIR="${HIDEVLAB_PERSIST_DIR:-/workspace/user_data}"
REPO_SCRIPTS="$SCRIPT_DIR"

# 兼容常见 authkey 变量名（Cloud Secret 常叫 TAILSCALE_AUTHKEY）
if [ -z "${TS_AUTHKEY:-}" ]; then
  if [ -n "${TAILSCALE_AUTHKEY:-}" ]; then TS_AUTHKEY="$TAILSCALE_AUTHKEY"
  elif [ -n "${TS_AUTHKEY_ALT:-}" ]; then TS_AUTHKEY="$TS_AUTHKEY_ALT"
  fi
fi
: "${TS_AUTHKEY:?请设置 TS_AUTHKEY（或 TAILSCALE_AUTHKEY）。不要贴到聊天；只在本机终端导出；须 reusable}"

echo "===== HiDevLab webide_start ====="
echo "time=$(date -u +%Y-%m-%dT%H:%M:%SZ) host=$(hostname) persist=$PERSIST_DIR"

# ---------- 0) 拷到持久盘（overlay 重启会丢 /workspace 根）----------
mkdir -p "$PERSIST_DIR"
for f in webide_start.sh hidevlab_ts.sh webide_boot.sh materialize_ssh_key.sh; do
  src=""
  if [ -f "$REPO_SCRIPTS/$f" ]; then
    src="$REPO_SCRIPTS/$f"
  elif [ -f "$PERSIST_DIR/$f" ]; then
    src="$PERSIST_DIR/$f"
  fi
  if [ -n "$src" ]; then
    # 若当前就是从 persist 跑、源=目标，跳过；否则刷新持久副本
    if [ "$(readlink -f "$src" 2>/dev/null || echo "$src")" != "$(readlink -f "$PERSIST_DIR/$f" 2>/dev/null || echo "$PERSIST_DIR/$f")" ]; then
      cp -f "$src" "$PERSIST_DIR/$f"
      chmod +x "$PERSIST_DIR/$f"
    fi
  fi
done
# 后续优先用持久副本（即使仓库树不在）
if [ -x "$PERSIST_DIR/hidevlab_ts.sh" ]; then
  TS_SCRIPT="$PERSIST_DIR/hidevlab_ts.sh"
else
  TS_SCRIPT="$REPO_SCRIPTS/hidevlab_ts.sh"
fi
if [ -x "$PERSIST_DIR/webide_boot.sh" ]; then
  BOOT_SCRIPT="$PERSIST_DIR/webide_boot.sh"
else
  BOOT_SCRIPT="$REPO_SCRIPTS/webide_boot.sh"
fi

# ---------- 1) CANN / 设备环境 ----------
if [ "${SKIP_BOOT:-0}" = "1" ]; then
  echo "[start] SKIP_BOOT=1，跳过 webide_boot"
else
  if [ -x "$BOOT_SCRIPT" ] || [ -f "$BOOT_SCRIPT" ]; then
    echo "[start] run webide_boot ..."
    # boot 用 set -e；失败不阻断入网（有时只是 npu-smi 慢）
    bash "$BOOT_SCRIPT" || echo "[start] WARN: webide_boot 非 0，继续入网"
  else
    echo "[start] WARN: 找不到 webide_boot.sh，跳过 CANN 配置"
  fi
fi

# ---------- 2) Tailscale userspace + loopback sshd ----------
# 静态包与 state 一律落持久盘，避免每次开机重下
export TS_AUTHKEY
export TS_HOME="${TS_HOME:-$PERSIST_DIR/tailscale}"
export TS_HOSTNAME="${TS_HOSTNAME:-hidevlab-npu}"
export WORKSPACE_ROOT="${WORKSPACE_ROOT:-/workspace}"
export HIDEVLAB_TS_LOG="${HIDEVLAB_TS_LOG:-$PERSIST_DIR/hidevlab_ts.log}"
# 与 hidevlab_ts.sh 的 HIDEVLAB_TS_ENV 对齐
export HIDEVLAB_TS_ENV="${HIDEVLAB_TS_ENV:-$PERSIST_DIR/hidevlab_tailscale.env}"
export HIDEVLAB_PERSIST_DIR="$PERSIST_DIR"

echo "[start] run hidevlab_ts (TS_HOME=$TS_HOME TS_HOSTNAME=$TS_HOSTNAME) ..."
bash "$TS_SCRIPT"
rc=$?
if [ "$rc" -ne 0 ]; then
  echo "!! hidevlab_ts 失败 rc=$rc；看 $HIDEVLAB_TS_LOG"
  exit "$rc"
fi

# 若 hidevlab_ts 写到了 /workspace/.hidevlab_tailscale.env，同步一份到持久盘
if [ -f /workspace/.hidevlab_tailscale.env ]; then
  cp -f /workspace/.hidevlab_tailscale.env "$PERSIST_DIR/hidevlab_tailscale.env" 2>/dev/null || true
fi
if [ -f "$PERSIST_DIR/hidevlab_tailscale.env" ]; then
  # shellcheck disable=SC1090
  . "$PERSIST_DIR/hidevlab_tailscale.env" 2>/dev/null || true
fi

echo
echo "===== READY（贴回 Agent，勿贴 TS_AUTHKEY）====="
echo "persist_script=$PERSIST_DIR/webide_start.sh"
echo "ts_hostname=${TS_HOSTNAME:-hidevlab-npu}"
echo "ts_ip=${ts_ip:-${TSIP:-unknown}}"
echo "sshd=127.0.0.1:2222 (via tailnet serve)"
echo "ssh_user=$(id -un)"
if [ -f "$PERSIST_DIR/hidevlab_tailscale.env" ]; then
  echo "--- env file ---"
  cat "$PERSIST_DIR/hidevlab_tailscale.env"
fi
echo
echo "下次开机（持久盘）："
echo "  TS_AUTHKEY='…' bash $PERSIST_DIR/webide_start.sh"
echo "Agent 侧连接："
echo "  bash scripts/hidevlab/agent_connect.sh"
echo "===== webide_start done ====="
