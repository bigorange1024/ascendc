# STATUS — fix-encrypt-skel-mix-chain-toy

| 项 | 状态 |
|----|------|
| 目的 | Encrypt 任务链骨架 toy：默认 stub→NTT→inner→[GATE]→INTT→magic；skipNtt 入口 AIC Wait(4)；**Host 折 μ / 设备 μ-stub**（TASK-005） |
| CPU | 非本线门禁（未作为验收） |
| SIM（TASK-001 基线） | **PASS**：无 GATE；kernel wall **3.640s**；tick **19663** |
| SIM（TASK-002 `SKEL_GATE=1`） | **PASS**：kernel wall **4.052s**；tick **19639**；`out[8]=0x04` |
| SIM（TASK-003 `SKEL_HEAVY=1` `SKEL_GATE=1`） | **PASS**：16×64×64 + 4 轮 1/3；wall **8.765s**；tick **54416**；**未挂死** |
| SIM（TASK-004 A `SKEL_SKIPNTT=0`） | **PASS**：wall **3.224s**；tick **20140**；`out[8]=0x04` |
| SIM（TASK-004 B `SKEL_SKIPNTT=1` `OMIT_SET4=0`） | **PASS**：wall **2.242s**；tick **11399**；`out[8]=0x14` |
| SIM（TASK-004 C `SKEL_SKIPNTT=1` `OMIT_SET4=1`） | **HANG** rc=**124**：`KERNEL_COMPUTE_BUDGET_SEC=60`；wall **60.078s** |
| SIM（TASK-005 A `SKIPNTT=1` `HOST_MU=1` `OMIT=0`） | **PASS**：wall **2.848s**；tick **11328**；`out[8]=0x15` |
| SIM（TASK-005 B `SKIPNTT=1` `HOST_MU=0` `OMIT=0`） | **PASS**：wall **2.654s**；tick **12268**；`out[8]=0x14` |
| SIM（TASK-005 C `SKIPNTT=1` `HOST_MU=1` `OMIT=1`） | **HANG** rc=**124**：budget **60s**；wall **60.354s** |
| magic | `SKELENC1`；`out[8]=0x04`（GATE）/`0x14`（设备μ）/`0x15`（Hostμ）/`0xA5`（基线）；`[9:]=0xA5` |
| CrossCore | flag **1** / **3**；GATE **4**↔**8**（`SKEL_GATE`）；skipNtt 入口 **Wait/Set(4)**（`SKEL_SKIPNTT`） |
| 模式名 | `SKEL_SKIPNTT`；`SKEL_OMIT_SET4`；`SKEL_HOST_MU`（默认 1：Host 折 μ 占位） |
| 禁令 | 无 SyncAll-during-Wait；无 SoftSync 双向汇合；无真 SHAKE；无碎写 GM / 滥 launch；**未改** stable Encaps |

验收命令（本线 SIM only；**串行**，禁并行）：

```bash
cd ascendc-tests/fix-encrypt-skel-mix-chain-toy
# Host 折 μ
SKEL_SKIPNTT=1 SKEL_HOST_MU=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 设备 μ-stub
SKEL_SKIPNTT=1 SKEL_HOST_MU=0 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 故障注入（预期 124）
KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_SKIPNTT=1 SKEL_HOST_MU=1 SKEL_OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
