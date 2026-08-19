#!/usr/bin/env bash
# npu_card_guard.sh — 借入实机 NPU：跑算子前检查卡是否干净；失败/超时后给出污染恢复步骤。
#
# 背景（qa/2026-08-03、08-05）：Encaps/Decaps 卡在 l18_l19 后 Ctrl+C/timeout 124 可能污染
# 同一 ASCEND_DEVICE_ID；换卡只是绕开，须 npu-smi 查残留或 reset。
#
# 用法（被 npu_run_case.sh / npu_kem_real_machine_suite.sh source）：
#   source "${REPO_ROOT}/scripts/npu_card_guard.sh"
#   npu_card_guard_preflight
#   … 跑算子 …
#   npu_card_guard_on_failure "${rc}" "encaps stable"
#
# 环境：
#   ASCEND_DEVICE_ID   显式优先；未设时由 npu_device_map / 调用方传入
#   NPU_GUARD_STRICT=1 有他人进程占用目标卡 → exit 2（默认 1）
#   NPU_GUARD_ALLOW_RESET_HINT=1 失败时打印 reset 提示（默认 1）

# shellcheck disable=SC2034
_NPU_CARD_GUARD_LOADED=1

npu_card_guard_device_id() {
    printf '%s\n' "${ASCEND_DEVICE_ID:-0}"
}

# 只读：npu-smi 是否可用、目标卡上是否有进程。
npu_card_guard_preflight() {
    local dev
    dev="$(npu_card_guard_device_id)"
    if ! command -v npu-smi >/dev/null 2>&1; then
        echo "[npu_guard] ERROR: 未找到 npu-smi，无法确认 NPU 环境" >&2
        return 2
    fi
    echo "[npu_guard] preflight device=${dev}"
    npu-smi info 2>&1 | head -20 || true
    echo "[npu_guard] --- Process 段（目标卡 ${dev} 须无残留）---"
    local proc_block
    proc_block="$(npu-smi info 2>/dev/null | sed -n '/Process/,$p' || true)"
    echo "${proc_block}"
    if [ "${NPU_GUARD_STRICT:-1}" = "1" ]; then
        # 粗检：Process 段出现非空行且含 PID/占用（各 CANN 版本格式略异，保守告警）
        if echo "${proc_block}" | grep -qE '[0-9]+'; then
            if echo "${proc_block}" | grep -qvE '^Process|^$|^\+|^\-|No running'; then
                echo "[npu_guard] WARN: npu-smi 报告可能有进程占用；建议 reset 或换 ASCEND_DEVICE_ID" >&2
                echo "[npu_guard]       继续跑 Encaps/Decaps 有连环挂风险。设 NPU_GUARD_STRICT=0 可强制继续。" >&2
                return 2
            fi
        fi
    fi
    return 0
}

# timeout 124 / 非零退出 / 用户中断后调用。
npu_card_guard_on_failure() {
    local rc="${1:-1}"
    local label="${2:-算子}"
    local dev
    dev="$(npu_card_guard_device_id)"
    echo "[npu_guard] ===== 失败善后（${label}，rc=${rc}）=====" >&2
    if [ "${rc}" -eq 124 ]; then
        echo "[npu_guard] exit 124 = KERNEL_COMPUTE_BUDGET 超时，常见于 l18_l19 挂死。" >&2
    fi
    echo "[npu_guard] 1) 勿在同卡立刻重跑 Encaps/Decaps/stable KEM E 段" >&2
    echo "[npu_guard] 2) npu-smi info | sed -n '/Process/,\$p'  确认 device=${dev} 无残留" >&2
    echo "[npu_guard] 3) 有残留 → 对该卡 reset，或 export ASCEND_DEVICE_ID=<干净卡>" >&2
    echo "[npu_guard] 4) 定位 l18：F203_L18_TRACE=1 仅 Encaps stable 单跑（干净卡）" >&2
    echo "[npu_guard] 5) 详见 qa/2026-08/2026-08-05-l18卡死初步诊断与实机最小实验.md" >&2
    if [ "${NPU_GUARD_ALLOW_RESET_HINT:-1}" = "1" ]; then
        echo "[npu_guard] reset 须管理员执行；Agent 禁止擅自 reset，只打印提示。" >&2
    fi
}

# 跑完 SUCCESS 后也建议看一眼 Process 段（可选）。
npu_card_guard_post_success() {
    local label="${1:-算子}"
    echo "[npu_guard] ${label} 退出 0；建议下一例前仍执行: npu-smi info | sed -n '/Process/,\$p'"
}
