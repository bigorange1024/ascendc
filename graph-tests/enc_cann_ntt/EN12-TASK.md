# EN12 — NPU 同进程 sticky 多轮（SampleNTT 贯通链）

> DAG：`D-EXP-EN12`  
> 基线：EN09 贯通链（含设备 SampleNTT）+ EN08 sticky 编排模式  
> 目标：同进程、同 acl session、不 recreate stream，整链重复 **R≥4**（默认 **16**，`EN12_ROUNDS` 可覆盖）；主门禁不挂；保持 NPU 忙碌

## 目录

`graph-tests/enc_cann_ntt/EN12-samplentt-sticky/`

自 **EN09-samplentt-device** 复制后改 Host 编排（勿抄 Encrypt/KEM/frozen/ER 核）。

## 允许阅读

- `EN09-samplentt-device/`（实现基线）
- `EN08-wired-sticky-rounds/main.cpp`（sticky 模式：缓冲一次分配、轮内复用、round_XX_done）
- `docs/notes/Encrypt-cann-ntt-kb.md`、capability inventory、DAG
- cann-ntt 迁入积木（本用例树内已有）

## 禁止

- 融胖 MIX GATE；抄 PKE/KEM 算子核；抄 `enc_related/ER0*` 核模板；从 frozen 带码
- 每轮 `aclInit` / 重建 stream（粘性语义）
- 未授权改 Rule/Skill

## 实现约束

1. 段序同 EN09：L0 SampleNTT → L1 Prep → L2 NTT → L3 Matvec → L4 INTT → L5 Pack  
2. Â 必须来自设备 SampleNTT（禁止 Host 随机冒充 Matvec Â）  
3. sticky：R 轮复用 GM/Host 缓冲；第 1 轮写 golden 对拍输出；后续轮可 mutate seed/nonce（对齐 EN08）  
4. `run.sh`：允许 `-r npu`（`ASCENDC_CASE_SUPPORTS_NPU=1`；默认 `ASCEND_DEVICE_ID=4` 可被 env 覆盖）；默认 CPU+SIM 仍验收  
5. 环境变量：`EN12_ROUNDS` 默认 **16**

## 验收

| 模式 | 命令 |
|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` |
| sync | 设备同步相关须跑 sync_audit；红线不得否决 |

主门禁：不挂（exit≠124；无 SynchronizeStream 卡死）。  
正确性：第 1 轮 SampleNTT+贯通 golden；失败标 `CORRECTNESS_SOFT_FAIL`。

## 反馈格式

`PASS-NOHANG` / `FAIL` / `BLOCKED` + 命令 + wall/tick + 红线条数 + 拟写入 KB 一句教训。
