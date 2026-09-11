# EN09 — 设备 SampleNTT（Â）接入（CPU+SIM）

> DAG：`D-EXP-EN09`  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN09-samplentt-device/`  
> 基线：复制 `EN08-wired-sticky-rounds/` 或 `EN07-pipeline-wired/`（推荐 EN07 单轮贯通壳以控 SIM 墙钟；勿改 EN01–08）  
> **仅 CPU+SIM**；主目标不挂。

---

## 1. 目标

将能力 **S1 Alg.7 SampleNTT** 落到设备侧，并接入 Host 编排：

| 项 | 要求 |
|----|------|
| 设备 | 至少生成 **k=4 的 Â 一行/一块**（或 STATUS 声明的子集）经 SHAKE+rej→`â[256]` |
| 接入 | Matvec 使用的 Â **来自本刀设备 SampleNTT 输出**（禁止仍用与设备无关的纯 Host 随机 Â 冒充） |
| Launch | SampleNTT **独立 AIV**（X32）；可并入 Prep launch 但须 STATUS 写清；禁融 NTT MIX |
| 主链 | 建议保留 Prep→NTT→Matvec→INTT→Pack 贯通；R 默认 **1**（控时） |

正确性尽量对拍；主门禁不挂。

---

## 2. 允许 / 禁止

允许：`F203-Alg7-SampleNTT-单poly技术总结.md`、shared SHAKE、alg7 探针 STATUS（禁整文件抄核）。  
禁止：抄 Encrypt prep；NPU；commit/push；改 KB/DAG；否决 sync 红线。

## 3. 验收

```bash
cd graph-tests/enc_cann_ntt/EN09-samplentt-device
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

STATUS 写清 Â 覆盖范围与是否喂入 Matvec；sync_audit；墙钟 ≤90min。

## 4. 反馈

```text
EN09: PASS-NOHANG | FAIL | BLOCKED | ABORT
dir: ...
samplentt: 覆盖范围一句
cpu/sim: ...
sync_audit: 红线=N path=…
lesson: 一句
```
