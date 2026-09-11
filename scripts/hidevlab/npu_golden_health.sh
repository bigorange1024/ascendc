#!/usr/bin/env bash
# HiDevLab：NPU「静默算错」健康检查（不依赖 npu-smi Process 段）。
#
# 背景：猎挂 SIGTERM 杀进程后，npu-smi 可显示无进程且 Health=OK，但简单加法算子
# 对拍已坏（前 4096 元正确、后段大片错）。只看 Process 段会假绿。
#
# 用法（在 HiDevLab 容器内、已 source CANN env）：
#   bash /workspace/ascendc/scripts/hidevlab/npu_golden_health.sh
#   HIDEVLAB_HEALTH_CASE=/path/to/add_custom bash …/npu_golden_health.sh
#
# 退出：0=对拍通过；1=对拍失败（判卡脏，须控制台关机→启动）；2=环境/编译错误
set -euo pipefail

CASE_DIR="${HIDEVLAB_HEALTH_CASE:-/workspace/ascendc/ascendc-tests/add_custom}"
SOC="${HIDEVLAB_HEALTH_SOC:-Ascend910B3}"
export ASCEND_DEVICE_ID="${ASCEND_DEVICE_ID:-0}"
export PATH="${PATH:-}"
# numpy：HiDevLab 系统 python 常无 numpy
if [ -d /usr/local/python3.12.13/bin ]; then
  export PATH="/usr/local/python3.12.13/bin:${PATH}"
fi
export LD_LIBRARY_PATH="/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:${LD_LIBRARY_PATH:-}"
set +u
for f in /usr/local/Ascend/cann-9.1.0/set_env.sh /usr/local/Ascend/cann/set_env.sh \
         /usr/local/Ascend/ascend-toolkit/set_env.sh; do
  if [ -f "$f" ]; then
    # shellcheck disable=SC1090
    source "$f" >/dev/null 2>&1 || true
    break
  fi
done
set -u

if [ ! -d "${CASE_DIR}" ]; then
  echo "[npu_health] ERROR: 用例目录不存在: ${CASE_DIR}" >&2
  exit 2
fi

echo "[npu_health] 跑简单加法算子对拍（add_custom）检测 NPU 是否静默算错"
echo "[npu_health] CASE=${CASE_DIR} ASCEND_DEVICE_ID=${ASCEND_DEVICE_ID} SOC=${SOC}"
cd "${CASE_DIR}"
LOG="${HIDEVLAB_HEALTH_LOG:-/tmp/npu_golden_health_$$.log}"
set +e
timeout "${HIDEVLAB_HEALTH_TIMEOUT_SEC:-300}" bash run.sh -r npu -v "${SOC}" >"${LOG}" 2>&1
rc=$?
set -e

if grep -q '\[SUCCESS\] output matches golden' "${LOG}"; then
  echo "[npu_health] OK：加法对拍通过，NPU 计算面看起来干净"
  exit 0
fi

echo "[npu_health] FAIL：加法对拍未过（rc=${rc}）。这通常不是 add_custom 本身坏了，" >&2
echo "[npu_health]       而是猎挂杀进程后的「静默卡脏」：进程表空、Health=OK，但结果错。" >&2
echo "[npu_health] 恢复：HiDevLab 控制台对该环境「关机 → 启动」，再跑 hidevlab_ts.sh。" >&2
echo "[npu_health]       容器内 npu-smi reset 不可用，Agent 无法代你清卡。" >&2
echo "[npu_health] --- 日志尾 ---" >&2
tail -20 "${LOG}" >&2 || true
exit 1
