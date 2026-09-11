#!/usr/bin/env bash
# 从「可能被 Dashboard / 环境变量弄坏格式」的私钥字符串，重建**合法** OpenSSH 私钥文件。
# 背景（2026-09-11）：Cloud Secret CANNLAB_SSH_KEY 多次被存坏——
#   先是只剩 base64 本体丢了 PEM 头尾；改了之后又变成「头+base64+尾用空格拼成一整行」，
#   两种 OpenSSH 都报 `Load key: error in libcrypto`。本脚本把这些坏格式统一修回可用私钥，
#   这样**即使 Secret 保持坏格式，任何消费方都能正常落私钥、连接**，无需再动 Dashboard。
#
# 兜底覆盖：
#   (a) 已是合法多行 PEM               → 原样校验通过即用
#   (b) 有 armor 但换行被压成空格(单行) → 剥掉头尾字面串、去所有空白得纯 base64、按 70 列重折
#   (c) 只有 base64 本体(无头尾)        → 同上补回头尾
#
# 用法：
#   bash scripts/hidevlab/materialize_ssh_key.sh [OUT]                 # 默认读 $CANNLAB_SSH_KEY
#   VAR_NAME=OTHER_KEY bash scripts/hidevlab/materialize_ssh_key.sh [OUT]
#   printf '%s' "$SOME_KEY" | bash scripts/hidevlab/materialize_ssh_key.sh [OUT]   # 从 stdin
# OUT 缺省 ~/.ssh/cannlab；成功后 chmod 600 并 `ssh-keygen -y` 打印公钥自证。
set -u

OUT="${1:-$HOME/.ssh/cannlab}"
# 默认按顺序尝试常见 Secret 名（用户当前用 CANNLAB_SSH_KEY）
if [ -z "${VAR_NAME:-}" ]; then
  for _cand in CANNLAB_SSH_KEY HIDEVLAB_SSH_KEY CURSOR_SSH_KEY; do
    if [ -n "${!_cand:-}" ]; then VAR_NAME="$_cand"; break; fi
  done
  VAR_NAME="${VAR_NAME:-CANNLAB_SSH_KEY}"
fi

# ---------- 取原始值：优先同名环境变量；否则读 stdin ----------
raw="${!VAR_NAME:-}"
if [ -z "$raw" ] && [ ! -t 0 ]; then raw="$(cat)"; fi
[ -n "$raw" ] || { echo "!! 空私钥：环境变量 ${VAR_NAME} 未设且 stdin 无输入" >&2; exit 2; }

# 去掉可能被整体包裹的一对引号与首尾空白（Dashboard 粘贴常见）
raw="${raw#"${raw%%[![:space:]]*}"}"   # 去前导空白
raw="${raw%"${raw##*[![:space:]]}"}"   # 去尾随空白
case "$raw" in
  \"*\") raw="${raw#\"}"; raw="${raw%\"}" ;;
  \'*\') raw="${raw#\'}"; raw="${raw%\'}" ;;
esac

HDR="-----BEGIN OPENSSH PRIVATE KEY-----"
FTR="-----END OPENSSH PRIVATE KEY-----"

mkdir -p "$(dirname "$OUT")" 2>/dev/null || true
chmod 700 "$(dirname "$OUT")" 2>/dev/null || true

# ---------- 情形 (a)：原样就是合法多行 PEM ----------
tmp="$(mktemp)"; trap 'rm -f "$tmp"' EXIT
printf '%s\n' "$raw" > "$tmp"
if grep -q "BEGIN OPENSSH PRIVATE KEY" "$tmp" && ssh-keygen -y -f "$tmp" >/dev/null 2>&1; then
  install -m 600 "$tmp" "$OUT"
  echo "[materialize] 原格式即合法（未改）-> $OUT"
else
  # ---------- 情形 (b)/(c)：剥 armor 字面串 → 去所有空白 → 纯 base64 → 70 列重折 ----------
  body="$(printf '%s' "$raw" | sed -e "s/${HDR}//g" -e "s/${FTR}//g" | tr -d ' \t\r\n')"
  [ -n "$body" ] || { echo "!! 去掉 armor 后 base64 为空，无法重建" >&2; exit 3; }
  { printf '%s\n' "$HDR"; printf '%s' "$body" | fold -w 70; printf '\n%s\n' "$FTR"; } > "$tmp"
  if ! ssh-keygen -y -f "$tmp" >/dev/null 2>&1; then
    echo "!! 重建后仍无法解析——原始值可能不是 OpenSSH 私钥，或 base64 本体已损坏" >&2
    exit 4
  fi
  install -m 600 "$tmp" "$OUT"
  echo "[materialize] 已重建（补/修 PEM 头尾 + 70 列折行）-> $OUT"
fi

echo "== 校验（应打印公钥，注释名 cursor-agent）=="
ssh-keygen -y -f "$OUT"
