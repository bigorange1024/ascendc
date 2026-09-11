# ER05 — 同 workspace 粘性双 COMPUTE 任务书

> 图谱节点：`D-EXP-ER05`  
> 目录（新建）：`graph-tests/enc_related/ER05-sticky-dual-compute/`  
> 前序：ER03/ER04 PASS — SIM 上 **Vec 加压**与 **Cube×16** 均不挂（X27/X28）。本刀换维。

## 假说

生产挂死可能与 **多 launch 粘性**（脏 workspace / CrossCore 残留 / 二次进 MIX）有关，而非单次体量。  
本刀：一次 PREP 后 **连续两次 COMPUTE**（同 GM workspace、同 TRACE 区可覆写），看 SIM 是否挂。

## 已锁参数

| 项 | 值 |
|----|-----|
| 脚手架 | 自 **ER04** 复制（含 GATE 256×32 + Cube×16 + ER02 同步） |
| Host launch | **3 次**：phase PREP → COMPUTE → **COMPUTE**（第二次 COMPUTE 不得再走 PREP） |
| TRACE | Host 打印须能区分两轮 COMPUTE（可在第二轮前 Host 清 TRACE 槽或使用偏移；须在 `trace_map.md`/`STATUS` 写清约定） |
| 核内 FSM/体量 | **不改** ER04 已锁核参数 |
| sync_audit | 红线 **0** |
| 正确性 | 非门禁 |

若 Host 壳无法第三次 launch：**停止改核参**，回报 blocked。

## 永禁

5/7；SyncAll@AIC-Wait；SoftSync；抄旧 Encrypt；假循环；commit/push；连 NPU。

## 墙钟

≤ **45min**；超时 ABORT。

## 验收

CPU + `SIM_DIRECT=1` sim；无 stray dump；`STATUS.md` + 反馈块。

```
ER05: PASS|FAIL|ABORT
dir: graph-tests/enc_related/ER05-sticky-dual-compute/
cpu: ...
sim: ...
sync_audit: 红线=N ...
blocked: ...
learned: ...
```
