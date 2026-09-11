# scripts/hidevlab — HiDevLab（昇腾在线开发）辅助

与 GitCode CANNLab（`scripts/cannlab/`）**分开维护**。

## 人只记这一行（WebIDE）

```bash
TS_AUTHKEY='你的key' bash /workspace/hidevlab_ts.sh
```

- **首次 / 新环境 / 丢盘**：自动下静态包 + userspace + 入网 + sshd:2222 + serve  
- **同一环境再开机**：自动跳过下载，只 rejoin  
- **禁止** `tailscale up --ssh`、禁止 apt 装 tailscale  
- key **不要**贴进聊天；跑完把 `/workspace/.hidevlab_tailscale.env` 贴回 Agent  

建议后台（防终端被掐）：

```bash
TS_AUTHKEY='你的key' nohup bash /workspace/hidevlab_ts.sh >/workspace/hidevlab_ts.log 2>&1 & echo PID=$!
# 崩了重连后：
tail -80 /workspace/hidevlab_ts.log
cat /workspace/.hidevlab_tailscale.env
```

若机上还没有 `/workspace/hidevlab_ts.sh`：让 Agent 经已有通道推一次，或从仓内拷贝  
`scripts/hidevlab/hidevlab_ts.sh`。

## Agent 侧

```bash
bash scripts/hidevlab/hidevlab_run.sh 'echo CONNECTED; npu-smi info | head -5'
# 跑 NPU 算子前建议先健康检查（猎挂后 Process 空仍可能静默算错）：
HIDEVLAB_NPU_HEALTH=1 bash scripts/hidevlab/hidevlab_run.sh 'cd /workspace/ascendc/… && bash run.sh -r npu -v Ascend910B3'
```

## 其它文件

| 文件 | 作用 |
|------|------|
| [`hidevlab_ts.sh`](hidevlab_ts.sh) | **人用一键**（权威） |
| [`agent_bootstrap_static.sh`](agent_bootstrap_static.sh) / [`agent_rejoin.sh`](agent_rejoin.sh) | 旧分步；优先用 `hidevlab_ts.sh` |
| [`lib_ssh.sh`](lib_ssh.sh) / [`which_npu.sh`](which_npu.sh) / [`hidevlab_run.sh`](hidevlab_run.sh) | Agent 长连 |
| [`npu_golden_health.sh`](npu_golden_health.sh) | 机上加法对拍：检出「静默卡脏」 |
| [`webide_boot.sh`](webide_boot.sh) | CANN 体检（与 Tailscale 无关） |

手册：[`docs/engineering/HiDevLab-WebIDE操作手册.md`](../../docs/engineering/HiDevLab-WebIDE操作手册.md)。
