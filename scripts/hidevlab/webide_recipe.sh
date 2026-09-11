#!/usr/bin/env bash
# HiDevLab WebIDE 配方打印机：只 stdout 可粘贴命令，不发起 SSH。
# 权威说明：docs/engineering/HiDevLab-WebIDE操作手册.md
#
# 用法：
#   bash scripts/hidevlab/webide_recipe.sh
#   bash scripts/hidevlab/webide_recipe.sh add_custom
#   bash scripts/hidevlab/webide_recipe.sh kem-keygen
#   bash scripts/hidevlab/webide_recipe.sh --branch <br> --case add_custom
#   bash scripts/hidevlab/webide_recipe.sh tailscale-bootstrap
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BRANCH="${HIDEVLAB_BRANCH:-}"
CASE="add_custom"
SOC_VERSION="${HIDEVLAB_SOC:-Ascend910B3}"
DEVICE_ID="${ASCEND_DEVICE_ID:-0}"
JOBS="${CMAKE_BUILD_JOBS:-8}"
WORKTREE="/workspace/ascendc"

usage() {
  cat <<'U'
Usage: webide_recipe.sh [--branch BR] [--case NAME] [CASE]
  CASE: add_custom (default) | kem-keygen | kem-encaps | kem-decaps | probe | tailscale-bootstrap
Env:
  HIDEVLAB_BRANCH  覆盖默认分支（否则用当前 git 分支或 main）
  HIDEVLAB_SOC     默认 Ascend910B3
  ASCEND_DEVICE_ID 默认 0（单卡逻辑号；勿填物理 davinci 号）
  CMAKE_BUILD_JOBS 默认 8
U
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --branch) BRANCH="$2"; shift 2 ;;
    --case) CASE="$2"; shift 2 ;;
    -*)
      echo "unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *) CASE="$1"; shift ;;
  esac
done

if [[ -z "$BRANCH" ]]; then
  if git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD >/dev/null 2>&1; then
    BRANCH="$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD)"
  else
    BRANCH="main"
  fi
fi

case "$CASE" in
  add_custom|smoke)
    REL="ascendc-tests/add_custom"
    EXPECT='[SUCCESS] output matches golden'
    ;;
  kem-keygen)
    REL="examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4"
    EXPECT='[verify] KEM KeyGen overall PASS  或  [SUCCESS]'
    ;;
  kem-encaps)
    REL="examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4"
    EXPECT='[SUCCESS] 或 verify PASS（注意粘性挂风险，先短超时）'
    ;;
  kem-decaps)
    REL="examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4"
    EXPECT='[SUCCESS] 或 verify PASS'
    ;;
  probe)
    REL="."
    EXPECT='npu-smi 显示 910B3 OK；ccec 在 PATH'
    ;;
  tailscale-bootstrap|ts-boot)
    REL="scripts/hidevlab"
    EXPECT='===== hidevlab bootstrap done ===== 且 2222 监听；把 .hidevlab_tailscale.env 贴回 Agent'
    ;;
  *)
    echo "unknown case: $CASE" >&2
    usage >&2
    exit 2
    ;;
esac

if [[ "$CASE" == "tailscale-bootstrap" || "$CASE" == "ts-boot" ]]; then
  echo "【HiDevLab WebIDE · Tailscale 开机一次】"
  echo "手册: docs/engineering/HiDevLab-WebIDE操作手册.md § Tailscale"
  echo
  echo "粘贴（一行，同 cannlab；**禁止**加 --ssh）："
  echo
  echo "<<<<<<< WEBIDE_PASTE"
  cat <<'EOF'
TS_AUTHKEY='tskey-auth-XXXX' bash /workspace/ascendc/scripts/hidevlab/agent_bootstrap.sh
cat /workspace/.hidevlab_tailscale.env
tailscale status | head -20
EOF
  echo ">>>>>>> WEBIDE_PASTE"
  echo
  echo "期望尾部关键字: ${EXPECT}"
  echo "回传: ts_ip=… 与 tailscale status 前几行"
  echo "终端崩: 见手册 §9.1；先 Reload Window，勿贴长块。"
  exit 0
fi

COMMON_ENV=$(cat <<EOF
source /usr/local/Ascend/cann/set_env.sh
export LD_LIBRARY_PATH=/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:\${LD_LIBRARY_PATH:-}
export ASCEND_DEVICE_ID=${DEVICE_ID}
export CANNLAB=1
export CMAKE_BUILD_JOBS=${JOBS}
EOF
)

echo "【HiDevLab WebIDE 配方】"
echo "分支: ${BRANCH}"
echo "手册: docs/engineering/HiDevLab-WebIDE操作手册.md"
echo "目录: ${WORKTREE}/${REL}"
echo
echo "粘贴:"
echo
echo "<<<<<<< WEBIDE_PASTE"
cat <<EOF
set -euo pipefail
cd ${WORKTREE}
git fetch origin '${BRANCH}'
git checkout '${BRANCH}'
git pull --ff-only origin '${BRANCH}'
${COMMON_ENV}
EOF

if [[ "$CASE" == "probe" ]]; then
  cat <<EOF
uname -m
npu-smi info | head -30
which ccec
ccec --version | head -2
ls -l /dev/davinci*
EOF
else
  cat <<EOF
cd ${WORKTREE}/${REL}
ASCEND_DEVICE_ID=${DEVICE_ID} CANNLAB=1 CMAKE_BUILD_JOBS=${JOBS} \\
  bash run.sh -r npu -v ${SOC_VERSION}
EOF
fi

echo ">>>>>>> WEBIDE_PASTE"
echo
echo "期望尾部关键字: ${EXPECT}"
echo "回传: 从编译结束到结尾的完整终端输出（可截断中间刷屏）"
echo
echo "说明: 主路径是 Tailscale（webide_recipe.sh tailscale-bootstrap）；本输出为人肉兜底单刀。"
