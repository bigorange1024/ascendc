#!/usr/bin/env bash
# HiDevLab WebIDE —— 每次开机只跑这一条（人侧）。
#
#   TS_AUTHKEY='tskey-…' bash /workspace/user_data/webide_start.sh
#   # 首次若只有仓库副本：
#   TS_AUTHKEY='tskey-…' bash /workspace/ascendc/scripts/hidevlab/webide_start.sh
#
# 默认 SKIP_BOOT=1（只起 tailnet/sshd，避免 npu-smi/set_env 拖慢）。
# 需要顺带配 CANN 时：BOOT=1 TS_AUTHKEY='…' bash …/webide_start.sh
#
# 若「停住不动」：几乎都是首次下载 pkgs.tailscale.com 被墙/超时。
#   看日志逐步输出；90s 内下不下来会报错退出（不再死等）。
#   预放包：把 tailscale_latest_<arch>.tgz 放到 /workspace/user_data/ 再跑。
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PERSIST_DIR="${HIDEVLAB_PERSIST_DIR:-/workspace/user_data}"

if [ -z "${TS_AUTHKEY:-}" ]; then
  if [ -n "${TAILSCALE_AUTHKEY:-}" ]; then TS_AUTHKEY="$TAILSCALE_AUTHKEY"
  elif [ -n "${TS_AUTHKEY_ALT:-}" ]; then TS_AUTHKEY="$TS_AUTHKEY_ALT"
  fi
fi
: "${TS_AUTHKEY:?请设置 TS_AUTHKEY（或 TAILSCALE_AUTHKEY）}"

# 默认跳过 boot（连接脚本优先）；显式 BOOT=1 才跑 webide_boot
if [ -n "${BOOT:-}" ] && [ "$BOOT" = "1" ]; then
  SKIP_BOOT=0
fi
SKIP_BOOT="${SKIP_BOOT:-1}"

echo "===== HiDevLab webide_start ====="
echo "time=$(date -u +%Y-%m-%dT%H:%M:%SZ) host=$(hostname) persist=$PERSIST_DIR SKIP_BOOT=$SKIP_BOOT"
echo "[start] 若长时间无新日志：多半卡在下载 Tailscale；默认 ${TS_CURL_MAX_SEC:-90}s 超时会退出"

mkdir -p "$PERSIST_DIR"
for f in webide_start.sh hidevlab_ts.sh webide_boot.sh materialize_ssh_key.sh; do
  src=""
  if [ -f "$SCRIPT_DIR/$f" ]; then src="$SCRIPT_DIR/$f"
  elif [ -f "$PERSIST_DIR/$f" ]; then src="$PERSIST_DIR/$f"
  fi
  if [ -n "$src" ]; then
    if [ "$(readlink -f "$src" 2>/dev/null || echo "$src")" != "$(readlink -f "$PERSIST_DIR/$f" 2>/dev/null || echo "$PERSIST_DIR/$f")" ]; then
      cp -f "$src" "$PERSIST_DIR/$f"
      chmod +x "$PERSIST_DIR/$f"
      echo "[start] refreshed $PERSIST_DIR/$f"
    fi
  fi
done

if [ -x "$PERSIST_DIR/hidevlab_ts.sh" ]; then TS_SCRIPT="$PERSIST_DIR/hidevlab_ts.sh"
else TS_SCRIPT="$SCRIPT_DIR/hidevlab_ts.sh"; fi
if [ -x "$PERSIST_DIR/webide_boot.sh" ]; then BOOT_SCRIPT="$PERSIST_DIR/webide_boot.sh"
else BOOT_SCRIPT="$SCRIPT_DIR/webide_boot.sh"; fi

if [ "$SKIP_BOOT" = "1" ]; then
  echo "[start] SKIP_BOOT=1，跳过 webide_boot（只要网络时推荐）"
else
  echo "[start] run webide_boot ..."
  if [ -f "$BOOT_SCRIPT" ]; then
    bash "$BOOT_SCRIPT" || echo "[start] WARN: webide_boot 非 0，继续入网"
  else
    echo "[start] WARN: 无 webide_boot.sh"
  fi
fi

export TS_AUTHKEY
export TS_HOME="${TS_HOME:-$PERSIST_DIR/tailscale}"
export TS_HOSTNAME="${TS_HOSTNAME:-hidevlab-npu}"
export WORKSPACE_ROOT="${WORKSPACE_ROOT:-/workspace}"
export HIDEVLAB_TS_LOG="${HIDEVLAB_TS_LOG:-$PERSIST_DIR/hidevlab_ts.log}"
export HIDEVLAB_TS_ENV="${HIDEVLAB_TS_ENV:-$PERSIST_DIR/hidevlab_tailscale.env}"
export HIDEVLAB_PERSIST_DIR="$PERSIST_DIR"

echo "[start] run hidevlab_ts (TS_HOME=$TS_HOME) … 逐步日志见下 / $HIDEVLAB_TS_LOG"
bash "$TS_SCRIPT"
rc=$?
if [ "$rc" -ne 0 ]; then
  echo "!! hidevlab_ts 失败 rc=$rc"
  echo "   日志: $HIDEVLAB_TS_LOG"
  echo "   daemon: $TS_HOME/tailscaled.log"
  if [ "$rc" = "10" ]; then
    echo "   → 下载超时。预放静态包后重跑："
    echo "     arch=\$(uname -m); case \$arch in aarch64) a=arm64;; *) a=amd64;; esac"
    echo "     # 在能上网的机器: curl -fL -o tailscale_latest_\$a.tgz https://pkgs.tailscale.com/stable/tailscale_latest_\$a.tgz"
    echo "     # 拷到盒子: /workspace/user_data/tailscale_latest_\$a.tgz"
  fi
  exit "$rc"
fi

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
echo "sshd=127.0.0.1:2222"
[ -f "$PERSIST_DIR/hidevlab_tailscale.env" ] && { echo "--- env ---"; cat "$PERSIST_DIR/hidevlab_tailscale.env"; }
echo "===== webide_start done ====="
