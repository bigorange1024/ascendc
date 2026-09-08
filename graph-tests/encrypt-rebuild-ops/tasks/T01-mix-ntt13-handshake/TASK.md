# T01 — MIX 最短 NTT 同构握手（flag 1/3）+ TRACE

| 字段 | 值 |
|------|-----|
| 状态 | **ready**（可派；无 NPU） |
| DAG | `D-EXP-T01` · 服务 `Q-ULT` / sync 脚手架 |
| 代码目录 | `graph-tests/toys/RB-T01-mix-ntt13-handshake/`（**新建**） |
| 运营目录 | `graph-tests/encrypt-rebuild-ops/tasks/T01-mix-ntt13-handshake/` |
| 墙钟 | ≤ 40 min（编码 + CPU + SIM ≤2 次） |

继承 [`../../COMMON.md`](../../COMMON.md)。

## 目标

1. `KERNEL_TYPE_MIX_AIC_1_2`（或本仓等价 MIX 1AIC+2AIV 壳）  
2. 仅一段与 Encrypt NTT **同构**的 CrossCore：  
   **AIV SET(1) → AIC WAIT(1) + 极轻 Cube → AIC SET(3) → AIV WAIT(3)**  
3. Host 单 launch + `SynchronizeStream`；约定 TRACE 打印  
4. **SIM 跑完不挂**（CPU 冒烟）

## 非目标

算法正确性；GATE/INTT；sampling；抄旧 Encrypt。

## 必读

- KB §B1 · §X1–X2  
- DAG：`C-SYNC-AUDIT`、`X-CROSSCORE-HANG`  
- cannbot：`api-crosscore-sync.md`  
- 壳参考（只读工程）：`ascendc-tests/pass-toy-mix-s123-byteencode-k2` 的 CMake/`run.sh` 形态（**勿抄业务核**）

## 验收

| # | 标准 |
|---|------|
| A1 | 仅新建本刀目录；未改禁抄树 |
| A2 | CPU + `SIM_DIRECT=1` sim **exit 0**，无 hang/timeout |
| A3 | `trace_map.md`：编号分区 Host/AIV0/AIV1/AIC |
| A4 | 日志可见 SET1 / WAIT1 / SET3 / sync 后 |
| A5 | `sync_audit` JSON 入 `logs/`；红线则 FAIL |
| A6 | 本运营目录 `FEEDBACK.md` 已填 |

## cannbot

编码后对代码目录跑 `sync_audit.py` → `logs/sync_audit.json`。

## 回报

写 `FEEDBACK.md`；实现树写 `STATUS.md`。
