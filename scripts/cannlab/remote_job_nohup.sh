#!/usr/bin/env bash
# 已弃用入口：请用 remote_job.sh（通用）或 run_npu_ab_nohup.sh（Encaps A/B）。
echo "Use: bash scripts/cannlab/remote_job.sh submit|poll|fetch" >&2
echo "  or: bash scripts/cannlab/run_npu_ab_nohup.sh submit|poll|fetch" >&2
echo "Keepalive: nohup bash scripts/cannlab/agent_link_keepalive.sh &" >&2
exit 1
