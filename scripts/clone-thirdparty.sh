#!/usr/bin/env bash
# clone-thirdparty.sh — 拉取本仓期望的可公开 clone 的 thirdparty/ 依赖
#
# 背景：根 .gitignore 排除 /thirdparty/，clone 本仓后须单独拉齐外部仓。
# 清单权威：docs/engineering/thirdparty-本地依赖.md
#
# Usage:
#   bash scripts/clone-thirdparty.sh              # 缺则 clone；已存在则跳过；默认顺带 build liboqs
#   FORCE=1 bash scripts/clone-thirdparty.sh      # 已存在也强制删后重拉（慎用）
#   ONLY=tiny_sha3,liboqs,ntt_onnx bash scripts/clone-thirdparty.sh
#   BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh   # 只 clone，不编 liboqs
#
# 新机器 / Cloud Agent 推荐：
#   git clone <本仓> && cd ascendc && bash scripts/clone-thirdparty.sh
#   # 等价：clone 后单独 bash scripts/build-liboqs.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TP="${REPO_ROOT}/thirdparty"
FORCE="${FORCE:-0}"
ONLY="${ONLY:-}"

mkdir -p "${TP}"

# name|url|ref|depth|notes
# ref 空 = 默认分支；depth 空 = 全历史；depth=1 = 浅克隆
REPOS=(
  "tiny_sha3|https://github.com/mjosaarinen/tiny_sha3.git||1|Host SHA3/SHAKE golden"
  "liboqs|https://github.com/open-quantum-safe/liboqs.git|0.15.0|1|ML-KEM KAT（须 tag 0.15.0）"
  "ascend-samples|https://gitee.com/ascend/samples.git||1|昇腾官方样例（体积大；仅参考）"
  "SHA3hp|https://openi.pcl.ac.cn/wtUSTB/SHA3hp.git||1|第三方 AscendC Keccak/SHA3"
  "cann-ntt|https://openi.pcl.ac.cn/serial2007/cann-ntt.git||1|第三方 AscendC 前向 NTT"
  "ntt_onnx|https://github.com/bigorange1024/ntt_onnx.git||1|NTT/LUT golden（**私有仓**；Cloud 须 ASCENDC_GH_PAT / gh 认证，见 thirdparty 文档）"
  "cannbot-skills|https://gitcode.com/cann/cannbot-skills.git||1|CANN Bot Skills（gitcode cann/cannbot-skills）"
)

should_process() {
  local name="$1"
  if [[ -z "${ONLY}" ]]; then
    return 0
  fi
  local item
  IFS=',' read -ra items <<< "${ONLY}"
  for item in "${items[@]}"; do
    [[ "${item}" == "${name}" ]] && return 0
  done
  return 1
}

clone_one() {
  local name="$1" url="$2" ref="$3" depth="$4" notes="$5"
  local dest="${TP}/${name}"

  if ! should_process "${name}"; then
    return 0
  fi

  echo "---- ${name} ----"
  echo "  url:   ${url}"
  echo "  notes: ${notes}"

  if [[ -d "${dest}/.git" ]] || [[ -d "${dest}" && -n "$(ls -A "${dest}" 2>/dev/null || true)" ]]; then
    if [[ "${FORCE}" != "1" ]]; then
      if [[ -d "${dest}/.git" ]]; then
        local cur
        cur="$(cd "${dest}" && git remote get-url origin 2>/dev/null || echo "(no origin)")"
        echo "  skip: already present (origin=${cur}); FORCE=1 to re-clone"
      else
        echo "  skip: directory exists but is not a git repo; FORCE=1 to replace"
      fi
      return 0
    fi
    echo "  FORCE=1: removing ${dest}"
    rm -rf "${dest}"
  fi

  local args=(clone)
  if [[ -n "${depth}" ]]; then
    args+=(--depth "${depth}")
  fi
  if [[ -n "${ref}" ]]; then
    args+=(--branch "${ref}")
  fi
  args+=("${url}" "${dest}")

  echo "  git ${args[*]}"
  # ntt_onnx 为私有仓：优先用 PAT（勿单独依赖 GH_TOKEN——Cursor Cloud 可能注入仅对本仓有效的 ghs_）
  if [[ "${name}" == "ntt_onnx" ]]; then
    local pat="${ASCENDC_GH_PAT:-${NTT_ONNX_GITHUB_TOKEN:-}}"
    if [[ -n "${pat}" ]]; then
      echo "  auth: ASCENDC_GH_PAT / NTT_ONNX_GITHUB_TOKEN（HTTPS x-access-token）"
      if git clone --depth "${depth:-1}" \
          "https://x-access-token:${pat}@github.com/bigorange1024/ntt_onnx.git" "${dest}"; then
        # 去掉 URL 里的 token，避免落入 .git/config
        git -C "${dest}" remote set-url origin "https://github.com/bigorange1024/ntt_onnx.git"
      else
        echo "  ERROR: authenticated HTTPS clone of private ntt_onnx failed" >&2
        return 1
      fi
    elif command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
      echo "  auth: gh repo clone（当前 gh 登录须能读 bigorange1024/ntt_onnx）"
      if ! gh repo clone bigorange1024/ntt_onnx "${dest}"; then
        echo "  ERROR: gh clone failed。Cloud：在 Secrets 配置 ASCENDC_GH_PAT（fine-grained，Contents:Read on ntt_onnx）" >&2
        return 1
      fi
    else
      if ! git "${args[@]}"; then
        echo "  ERROR: ntt_onnx 为**私有仓**，匿名 HTTPS 不可用。" >&2
        echo "  Cloud Agent：Dashboard → Cloud Agents → Secrets 增加 ASCENDC_GH_PAT=（fine-grained PAT，" >&2
        echo "    仓库 bigorange1024/ntt_onnx → Contents: Read）。勿仅用 GH_TOKEN（易被 Cursor 覆盖为 ghs_）。" >&2
        echo "  本机：gh auth login 后重跑，或 SSH：git clone git@github.com:bigorange1024/ntt_onnx.git" >&2
        return 1
      fi
    fi
  else
    if ! git "${args[@]}"; then
      echo "  ERROR: git clone failed for ${name} (${url})" >&2
      return 1
    fi
  fi

  if [[ -n "${ref}" ]]; then
    local got
    got="$(cd "${dest}" && git describe --tags --exact-match 2>/dev/null || git rev-parse --short HEAD)"
    echo "  checked out: ${got}"
  else
    echo "  HEAD: $(cd "${dest}" && git log -1 --oneline)"
  fi
}

echo "[clone-thirdparty] REPO_ROOT=${REPO_ROOT}"
echo "[clone-thirdparty] target=${TP}"
echo

for row in "${REPOS[@]}"; do
  IFS='|' read -r name url ref depth notes <<< "${row}"
  clone_one "${name}" "${url}" "${ref}" "${depth}" "${notes}"
  echo
done

echo "[clone-thirdparty] done."
echo
echo "注意：原 thirdparty/merged_kyber 已迁至 ascendc-tests/pass-merged-kyber-mix-ntt256/（勿再 clone 到 thirdparty）"
echo "注意：ntt_onnx 为**私有仓**；Cloud 须 Secrets 中的 ASCENDC_GH_PAT（见 docs/engineering/thirdparty-本地依赖.md）"

# 默认编译 liboqs（golden / KAT 依赖）；ONLY 排除 liboqs 或不想编时可 BUILD_LIBOQS=0
BUILD_LIBOQS="${BUILD_LIBOQS:-1}"
if [[ "${BUILD_LIBOQS}" = "1" ]] && should_process "liboqs"; then
  if [[ -d "${TP}/liboqs" ]]; then
    echo
    echo "[clone-thirdparty] building liboqs (BUILD_LIBOQS=1)…"
    bash "${SCRIPT_DIR}/build-liboqs.sh"
  else
    echo "[clone-thirdparty] WARN: thirdparty/liboqs missing; skip build" >&2
  fi
else
  echo
  echo "[clone-thirdparty] skip liboqs build (BUILD_LIBOQS=${BUILD_LIBOQS})"
  echo "需要 golden/KAT 时再跑: bash scripts/build-liboqs.sh"
fi

