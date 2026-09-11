#!/usr/bin/env bash
# ops_npu_watchdog.sh — 本机实时监听云 NPU；完成立刻衔尾 + 刷 NPU_LIVE（不靠用户催）
# nohup bash graph-tests/decrypt-rebuild-ops/ops_npu_watchdog.sh >>/tmp/ops_npu_watchdog.log 2>&1 &
set -uo pipefail
export TS_SOCK="${TS_SOCK:-$HOME/.local/share/tailscale/tailscaled.sock}"
REPO="${REPO:-/home/yuanye/ascendc}"
LIVE="${REPO}/graph-tests/decrypt-rebuild-ops/NPU_LIVE.md"
STATE="${REPO}/graph-tests/decrypt-rebuild-ops/.watch_state"
INTERVAL="${WATCH_INTERVAL:-15}"
cd "${REPO}"
# shellcheck source=/dev/null
source scripts/cannlab/lib_ssh.sh

launched_k02=0
launched_post=0
[[ -f "${STATE}" ]] && # shellcheck disable=SC1090
  source "${STATE}" || true

save_state() {
  printf 'launched_k02=%s\nlaunched_post=%s\n' "${launched_k02}" "${launched_post}" > "${STATE}"
}

write_live() {
  cat > "${LIVE}" <<EOF
# NPU 实时状态（**watchdog 每 ${INTERVAL}s 自动刷**）

> 刷新：$(date -Iseconds)

| 项 | 值 |
|----|-----|
| 当前 | $1 |
| 最近 | $2 |
| 下一刀 | $3 |
| 看门狗 | \`ops_npu_watchdog.sh\` · 日志 \`/tmp/ops_npu_watchdog.log\` |
EOF
}

ensure_keepalive() {
  pgrep -f 'scripts/cannlab/agent_link_keepalive.sh' >/dev/null 2>&1 && return 0
  nohup bash "${REPO}/scripts/cannlab/agent_link_keepalive.sh" >>/tmp/cannlab_keepalive.log 2>&1 &
}

remote() {
  local out rc
  cannlab_ssh_base sshc
  # 单行命令更稳；失败不吞 stderr
  out=$("${sshc[@]}" "$@" 2>/tmp/ops_watch_ssh.err) && rc=0 || rc=$?
  if [[ $rc -ne 0 ]]; then
    echo "SSH_FAIL rc=$rc $(tr '\n' ' ' </tmp/ops_watch_ssh.err | head -c 200)"
    return "$rc"
  fi
  printf '%s\n' "$out"
}

launch_k02() {
  cannlab_scp_opts scpo
  [[ -f /tmp/k02_x30_npu_job.sh ]] || return 1
  scp "${scpo[@]}" /tmp/k02_x30_npu_job.sh "developer@${SSH_HOST}:/tmp/k02_x30_npu_job.sh"
  remote 'chmod +x /tmp/k02_x30_npu_job.sh
    pgrep -f "/tmp/k02_x30_npu_job" >/dev/null && echo ALREADY || {
      nohup bash /tmp/k02_x30_npu_job.sh >/tmp/k02_x30_nohup.out 2>&1 & echo K02_JOB=$!
    }'
}

launch_k04_smoke() {
  cannlab_scp_opts scpo
  [[ -f /tmp/k04_x10_smoke_job.sh ]] || {
    # 最小内嵌
    cat > /tmp/k04_x10_smoke_job.sh <<'INNER'
#!/usr/bin/env bash
set -euo pipefail
exec > >(tee /tmp/rb-k04-x10-smoke.log) 2>&1
echo "START_K04_X10_SMOKE $(date -Iseconds)"
set +u; source /home/developer/Ascend/ascend-toolkit/set_env.sh
_ASCEND_INSTALL_PATH="${ASCEND_HOME_PATH:-/home/developer/Ascend/cann-9.0.0}"; set -euo pipefail
ROOT=/mnt/workspace/ascendc-drw-d04; CASE="${ROOT}/graph-tests/dec_related/RB-D07-enc-decaps-rt"; cd "${CASE}"
export ASCEND_DEVICE_ID=0 KERNEL_COMPUTE_BUDGET_SEC=180
export LD_LIBRARY_PATH="${CASE}/out/lib:${CASE}/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:${ROOT}/thirdparty/liboqs/build/lib:${LD_LIBRARY_PATH:-}"
[[ -x ./ascendc_kernels_bbit && -f ./out/lib/libascendc_kernels_npu.so ]] || flock /mnt/workspace/.npu.lock -c "ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3"
ok=0; fail=0
for i in $(seq 1 10); do
  echo "=== K04smoke $i/10 $(date -Iseconds) ==="; touch /mnt/workspace/.agent_heartbeat
  if flock /mnt/workspace/.npu.lock -c "bash -lc \"cd ${CASE} && source /home/developer/Ascend/ascend-toolkit/set_env.sh && export ASCEND_DEVICE_ID=0 KERNEL_COMPUTE_BUDGET_SEC=180 LD_LIBRARY_PATH=${CASE}/out/lib:${CASE}/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:${ROOT}/thirdparty/liboqs/build/lib:\\\$LD_LIBRARY_PATH && python3 scripts/gen_data.py && bash ${ROOT}/scripts/kernel-run-timeout.sh ./ascendc_kernels_bbit && python3 scripts/verify_result.py\""; then
    ok=$((ok+1)); echo OK_$i
  else fail=$((fail+1)); echo FAIL_$i; fi
done
echo "ALL_DONE_K04_X10_SMOKE ok=$ok fail=$fail $(date -Iseconds)"
INNER
  }
  scp "${scpo[@]}" /tmp/k04_x10_smoke_job.sh "developer@${SSH_HOST}:/tmp/k04_x10_smoke_job.sh"
  remote 'chmod +x /tmp/k04_x10_smoke_job.sh
    pgrep -f k04_x10_smoke >/dev/null && echo ALREADY || {
      nohup bash /tmp/k04_x10_smoke_job.sh >/tmp/k04_x10_nohup.out 2>&1 & echo SMOKE=$!
    }'
}

echo "[watch] start $(date -Iseconds)" | tee -a /tmp/ops_npu_watchdog.log

while true; do
  ensure_keepalive
  if ! cannlab_pick_host; then
    write_live "host_offline" "-" "wait_bootstrap"
    sleep "${INTERVAL}"; continue
  fi
  snap=$(remote 'echo TS=$(date -Iseconds)
    echo K01=$(grep -E "ALL_DONE_K01|K01_OK|ABORT" /tmp/rb-k01-x30-npu.log 2>/dev/null | tail -1)
    echo K02=$(grep -E "ALL_DONE_K02|K02_OK|START|ABORT" /tmp/rb-k02-x30-npu.log 2>/dev/null | tail -1)
    echo K04=$(grep -E "ALL_DONE_K04_X10|OK_|START|FAIL" /tmp/rb-k04-x10-smoke.log 2>/dev/null | tail -1)
    pgrep -af "/tmp/k01_x30|/tmp/k02_x30|/tmp/k04_x10|ascendc_kernels_bbit" | grep -v pgrep | head -4 || echo PROC_IDLE
  ' 2>/dev/null || echo SSH_FAIL)

  echo "$(date -Iseconds) ${snap}" >> /tmp/ops_npu_watchdog.log
  printf '%s\n' "${snap}" > /tmp/ops_npu_snap.txt

  last=$(echo "${snap}" | grep -E '^K0' | tail -1)

  if echo "${snap}" | grep -q k01_x30_npu_job; then
    write_live "**K01×30 RUNNING**" "${last}" "K02×30"
  elif echo "${snap}" | grep -q ALL_DONE_K01 && ! echo "${snap}" | grep -q k02_x30_npu_job && [[ "${launched_k02}" -eq 0 ]]; then
    if ! echo "${snap}" | grep -q ALL_DONE_K02; then
      echo "[watch] launch K02 $(date -Iseconds)" | tee -a /tmp/ops_npu_watchdog.log
      launch_k02 && launched_k02=1 && save_state
      write_live "**K02 LAUNCHED**" "${last}" "K04 smoke"
    fi
  elif echo "${snap}" | grep -q k02_x30_npu_job; then
    launched_k02=1; save_state
    write_live "**K02×30 RUNNING**" "${last}" "K04×10 smoke"
  elif echo "${snap}" | grep -q ALL_DONE_K02 && [[ "${launched_post}" -eq 0 ]]; then
    echo "[watch] launch K04 smoke $(date -Iseconds)" | tee -a /tmp/ops_npu_watchdog.log
    launch_k04_smoke && launched_post=1 && save_state
    write_live "**K04 smoke LAUNCHED**" "${last}" "刷 STATUS/KB"
  elif echo "${snap}" | grep -q k04_x10_smoke; then
    write_live "**K04×10 smoke RUNNING**" "${last}" "沉淀"
  elif echo "${snap}" | grep -q ALL_DONE_K04_X10; then
    write_live "**矩阵加压收口**" "${last}" "Agent 刷文档（可放机）"
  else
    write_live "idle/unknown" "${last:-see snap}" "见 /tmp/ops_npu_snap.txt"
  fi

  sleep "${INTERVAL}"
done
