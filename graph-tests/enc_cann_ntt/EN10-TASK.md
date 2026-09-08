# EN10 — NPU 真机验收 EN09 贯通链（不卡死）

> DAG：`D-NPU-WAIT-ULT` / `D-EXP-EN10`  
> 基线目录：远程运行 `EN09-samplentt-device`（可从 Cloud 同步）  
> **目标**：`aclrtSynchronizeStream` **不卡死**；正确性次要。

## 约束

- 机型：**910B3**（`npu-smi`）；`-v Ascend910B3`  
- 设备节点：`/dev/davinci4` → `ASCEND_DEVICE_ID=4`  
- 保活：SSH `ServerAliveInterval≤20`；作业间隙 **<4 min**（用户：超时可能自动关机）  
- 发现主机：`100.68.205.47` / MagicDNS `cannlab-npu`（经 Tailscale SOCKS；勿写死其它 IP）  
- 禁止：长时间空闲；改稳定 Encrypt 生产核；无授权 commit/push  

## 验收

```bash
cd graph-tests/enc_cann_ntt/EN09-samplentt-device   # 或同步后的等价路径
ASCEND_DEVICE_ID=4 bash run.sh -r npu -v Ascend910B3
```

（须临时解除本刀历史「禁 npu」门禁，仅用于本 EN10 真机跑。）

## 反馈

NPU: PASS-NOHANG | FAIL-HANG | FAIL-OTHER | BLOCKED  
wall / 是否 SynchronizeStream 卡住 / 末行日志  
