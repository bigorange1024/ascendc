# EN14-encrypt-cross-sticky · STATUS

> DAG：`D-EXP-EN14`  
> 日期：2026-09-11  
> 结论：**PASS**（CPU + `SIM_DIRECT=1` sim；R=8 每轮 `c`≡liboqs **max=0**）

---

## 1. 目标

同 session sticky：EN13 Encrypt×liboqs 整链重复 R=8；每轮换种子后仍 `c`≡liboqs；不挂。

## 2. 相对 EN13

| 项 | 本刀 |
|----|------|
| 核 | **同 EN13**（未改设备核） |
| 编排 | 对齐 EN12 sticky：一次 aclInit/stream/缓冲，R 轮复用 |
| 种子 | `SEED_D+(r-1)*10007`；`input/rXX/` 分轮输入 |
| 门禁 | 每轮 `output/rXX/c.bin` ≡ liboqs |

## 3. 验收

```bash
EN14_ROUNDS=8 bash run.sh -r cpu -v Ascend910B4
EN14_ROUNDS=8 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | wall | tick | c max（每轮） |
|------|------|------|------|---------------|
| CPU | 0 | **9.573s** | — | **0** ×8 |
| SIM | 0 | **1046.520s** | **6455278**（≈807k/轮） | **0** ×8 |

日志：`/opt/cursor/artifacts/enc-encaps-sim/en14-cpu.log`、`en14-sim.log`。  
用例根无 stray `core*.dump`；SIM stray 收拢至 `sim_log/`。

## 4. sync_audit

设备核相对 EN13 **未改**；复用 EN13 审计结论：红线 **0**。  
json：`sync_audit.json`（自 EN13 复制壳保留）。

## 5. 已锁参数

同 EN13；另锁 `EN14_ROUNDS` 默认 **8**；禁 `-r npu`。
