# 2026-09-10 · HiDevLab WebIDE 主路径

## 决策

- HiDevLab 云 NPU：**不以 SSH直连为主**（跳板命令约 5 分钟有效，不适合 Cloud Agent）。
- **主路径 = WebIDE + Git 配方协作**；独立手册维护，不与 CANNLab/Tailscale 手册混写。

## 落盘

- 手册：`docs/engineering/HiDevLab-WebIDE操作手册.md`
- 脚本：`scripts/hidevlab/webide_recipe.sh`（只打印可贴命令）
- 入口：`docs/engineering/INDEX.md`、`AGENTS.md`、`AGENT_HANDOFF.md`

## 环境基线（已创建）

- `DevEnv_185447`：A2 · CANN 9.1.0 ubuntu · 1 NPU 算子调测
- 实测：Ubuntu 22.04 aarch64；CANN 9.1.0；910B3；`/dev/davinci5`；逻辑设备仍 `ASCEND_DEVICE_ID=0`
- 工程目录约定：`/workspace/ascendc`

## 下一刀

WebIDE 内跑 `webide_recipe.sh add_custom` 冒烟，回传日志。

## 同日追加：add_custom 真机冒烟 PASS

- 容器 `ede7a5a508b2`；物理 NPU **7** / `/dev/davinci7`；逻辑仍 `ASCEND_DEVICE_ID=0`
- 分支 `cursor/hidevlab-cloud-npu-9099@3f2c2ed`
- `ASCEND_DEVICE_ID=0 CANNLAB=1 bash run.sh -r npu -v Ascend910B3`
- 结果：`[SUCCESS] output matches golden (Ascend910B3)`（md5 与 golden 一致）
- 备注：`runtime_env` 曾打印 `soc=Ascend910B4`，以显式 `-v Ascend910B3` 为准已正确上板

## 同日追加：Tailscale 长连 + EN12 HiDevLab NPU

- 路径：静态包 + **userspace**（无 TUN）+ `sshd 127.0.0.1:2222` + `tailscale serve --tcp 2222`；人侧一键 `/workspace/hidevlab_ts.sh`
- Agent 经 SOCKS/DERP 免密驱动；**禁** `--ssh`（崩 WebIDE）；**禁** apt 装 Tailscale（依赖锁死）
- 系统 `python3` 无 numpy → 须 `PATH=/usr/local/python3.12.13/bin:$PATH`（已写入 `webide_boot` / `hidevlab_env.sh`）
- **EN12 sticky NPU**（`ASCEND_DEVICE_ID=0` `-v Ascend910B3`）：
  - R=4 / R=16：早先 `[SUCCESS]` golden；kernel wall≈3.7s
  - R=64：NOHANG；猎挂后出现 CORRECTNESS_SOFT_FAIL（见下）

## 同日追加：Q-OLD-L18 对照（只读跑 stable，不改核）

- stable **PKE Encrypt**：单次 + 进程×8 → **不挂**、c golden（`/workspace/old_encrypt_npu_loop.log`）
- stable **KEM Encaps**：R1 SUCCESS(~2s) → **R2 卡在 `f203_encrypt_l18_l19`**（NPU 占卡；SIGTERM 后 rc=143）
- 结论：`Q-OLD-L18-STILL-HANG` = **仍会挂**（Encaps 多轮）；已写 KB X42 + DAG `I-OLD-L18-ENCAPS-STILL-HANG`
- **副作用**：猎挂杀进程后 `add_custom` NPU golden 失败、EN12 NPU soft-fail；**CPU 孪生仍绿**；`npu-smi reset` 容器不可用 → **须控制台重启环境**

## 同日追加：继续修（白话）· 卡仍脏 · 防误判

**「EN12」是什么**：`graph-tests/enc_cann_ntt/EN12-samplentt-sticky/` —— 把「采样矩阵Â → 噪声Prep → NTT → 矩阵向量乘 → INTT → 打包密文」六段，在**同一进程、同一 ACL 会话**里连跑多轮（粘性多轮）的试验用例。不是随便一个编号。

**当前阻塞（不是算子逻辑新 bug）**：

1. 猎挂杀旧 Encaps 后，NPU **静默算错**：`npu-smi` 显示无进程、Health=OK，但**简单加法**对拍仍失败（前 4096 个数对、后面大片错）。
2. 同一脏卡上，六段贯通链里「采样/Prep/NTT」常还能对，「矩阵向量乘 / INTT / 打包」会错 —— 这是污染画像，不是新发现的 Matvec 源码回归（猎挂前 R=4/R=16 曾整链 golden 绿）。
3. 容器内 **不能** `npu-smi reset`；Agent **无法**代清卡。

**已做的代码/脚本修复（不等重启也能合）**：

- 贯通链 Host（EN12 `main.cpp`）补上 `DeviceGuard`：中途 return / SIGTERM 时尽量 `ResetDevice+Finalize`（不能保证清干净 Cube，但比裸退好）。CPU 孪生 R=2 仍全段对拍绿。
- 新增 `scripts/hidevlab/npu_golden_health.sh`：用加法对拍检出「静默卡脏」；`HIDEVLAB_NPU_HEALTH=1` 可挂到 `hidevlab_run.sh` 上，脏卡时直接拦后续 NPU 命令。

**请你做的一步**：HiDevLab 控制台对该环境 **关机 → 启动** → `bash /workspace/hidevlab_ts.sh`。清卡后我才能继续验 NPU 正确性 / 再碰旧 Encaps 对照。

## 同日追加（09-11）：空转看门狗责任

- **有**：曾默认拉起 `agent_watchdog.sh`，并靠 `/workspace/.agent_heartbeat` + 误判逻辑「保活」。
- **为何没自停**：`npu-smi` 失败被当成 active，且 WebIDE idle shell 也被当成 active → 闲置计数永不累加。
- **现状**：Agent 侧 Tailscale 已看不到在线节点；**请控制台确认是否仍在计费并关机**。若 WebIDE 还能进，可先：
  `kill "$(cat /tmp/hidevlab_watchdog.pid 2>/dev/null)" 2>/dev/null; rm -f /workspace/.agent_heartbeat /tmp/hidevlab_watchdog.pid`
- **已改**：bootstrap **默认不拉看门狗**（须 `ENABLE_WATCHDOG=1`）；修 npu-smi 误判；去掉 pts 判定；`hidevlab_run` 默认不再 touch 心跳。

