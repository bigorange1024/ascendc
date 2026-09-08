# N00 — NPU 连通 + 短冒烟保活（非 Encrypt 新核）

| 字段 | 值 |
|------|-----|
| 状态 | **ok**（2026-09-08：SSH 通 + worktree + add_custom NPU SUCCESS；见 FEEDBACK） |
| 代码目录 | 复用远程已有 `ascendc-tests/add_custom`（独立 worktree） |
| 运营目录 | `graph-tests/encrypt-rebuild-ops/tasks/N00-npu-warm-smoke/` |
| 墙钟 | **≤ 20 min**（超时立刻 STOP 交 FEEDBACK，禁止空等） |
| 设备 | 910B3 · **`ASCEND_DEVICE_ID=0`** |

继承 [`../../COMMON.md`](../../COMMON.md) · [`../../RHYTHM.md`](../../RHYTHM.md)。

## 目标

1. 经 Tailscale SSH 连上 `developer@100.97.98.72:2222`（或 MagicDNS `cannlab-npu`）。  
2. 准备独立树：`/mnt/workspace/ascendc-encrypt-rebuild`（clone 或 worktree；与默认 `ascendc` 分离）。  
3. 短冒烟：`ASCEND_DEVICE_ID=0` 跑 `add_custom` `-r npu`（已知绿路径）。  
4. 写清连通步骤与耗时，证明本轨**不空等**。

## 非目标

- 新写 Encrypt / MIX 实验核  
- 等待 SIM T01  
- 长时间 sleep / 无命令挂机  
- `kill -TERM 1`

## 连接（本侧 WSL / Agent）

若环境已有 Secret：

```bash
# userspace tailscale + SOCKS，见 docs/engineering/CANNLab接入与远程驱动.md §4
SSH='ssh -o ProxyCommand="nc -X 5 -x 127.0.0.1:1055 %h %p" -o StrictHostKeyChecking=accept-new -o ServerAliveInterval=15 -i ~/.ssh/cannlab -p 2222 developer@100.97.98.72'
```

若 **无** `TAILSCALE_AUTHKEY` / 私钥：在 **2 分钟内** `BLOCKED` 回报，列出缺项；**不要重试空转**。可在本仓只写连通检查清单到 `logs/`。

## 远程短任务（连通后）

```bash
flock /mnt/workspace/.npu.lock -c '
  export LD_LIBRARY_PATH=/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:$LD_LIBRARY_PATH
  source /home/developer/Ascend/ascend-toolkit/set_env.sh
  # 若尚无独立树：从 /mnt/workspace/ascendc worktree 或 git clone
  cd /mnt/workspace/ascendc-encrypt-rebuild/ascendc-tests/add_custom || cd /mnt/workspace/ascendc/ascendc-tests/add_custom
  ASCEND_DEVICE_ID=0 CMAKE_BUILD_JOBS=8 bash run.sh -r npu -v Ascend910B3
'
```

## 验收

| # | 标准 |
|---|------|
| A1 | ssh 通或明确 BLOCKED（缺密钥/入网）≤2 min |
| A2 | 若通：add_custom NPU SUCCESS 或明确失败日志（非 hang 空等） |
| A3 | FEEDBACK + logs 摘录；墙钟 ≤20 min |
| A4 | 未跑任何未 SIM 验证的 Encrypt FSM |

## 回报

`FEEDBACK.md`：`npu: ok|blocked_auth|fail` + ts_ip + 是否 worktree 就绪。
