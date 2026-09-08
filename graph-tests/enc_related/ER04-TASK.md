# ER04 — NTT/INTT 真 Cube 多轮加压任务书

> 图谱节点：`D-EXP-ER04`  
> 目录（新建）：`graph-tests/enc_related/ER04-cube-ntt-volume/`  
> 主控派 Task subagent；主控不写核。  
> 前序：ER03 PASS — **仅 GATE Vec 加压不足以 SIM 复现挂**（X27）。

## 假说

生产路径在 NTT/INTT 段有 **真 Cube 体量**，不只 GATE 上 AIV Vec。本刀在 ER03 壳上对 **AIC 真 Mmad 多轮加压**，看 CAModel 是否仍不挂。

## 已锁参数（不得擅自改）

| 项 | 值 |
|----|-----|
| 脚手架 | 自 **ER03** 目录复制（含 MAC 256×32 + ER02 同步纪律） |
| GATE MAC | **保持** `kMacElems=256`，`kMacRounds=32`（不回退、不再加） |
| Cube 几何 | 仍 `C[16,32]=A[16,32]@B[32,32]` |
| Cube 轮数 | NTT 段与 INTT 段各 **`kCubeRounds=16`** 次真 `Mmad`（禁空转 for；每次完整 Init/Process 或同对象 Process×16，择一写清） |
| 外形 / flag | 2launch；GATE 4/8；NTT/INTT **1/3**；禁 5/7 |
| 同步 | EnQue/DeQue + `PIPE_V`；红线目标 **0** |
| 正确性 | 非门禁 |

UB/L1/编译放不下或 SIM 预算不够：**停止改参**，回报主控。

## 必用 cannbot

编码后 `sync_audit.py`；红线须修到 0，禁否决。

## 永禁

5/7；Wait 中 SyncAll；SoftSync；抄旧 Encrypt；假循环；commit/push/开分支；连 NPU。

## 墙钟

≤ **45min**；超时 ABORT。

## 验收

CPU + `SIM_DIRECT=1` sim 双过；无 stray dump；`STATUS.md` + 反馈块。

## 回写格式

```
ER04: PASS|FAIL|ABORT
dir: graph-tests/enc_related/ER04-cube-ntt-volume/
cpu: ...
sim: ...
sync_audit: 红线=N ...
blocked: ...
learned: ...
```
