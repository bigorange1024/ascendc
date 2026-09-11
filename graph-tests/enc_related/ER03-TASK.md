# ER03 — GATE 真积木体量加压任务书

> 图谱节点：`D-EXP-ER03`  
> 目录（新建）：`graph-tests/enc_related/ER03-gate-realbrick-volume/`  
> 主控派 **Task subagent** 编码；主控不写核实现。  
> 基线：ER01 外形 + ER02 同步纪律（红线 0）。

## 假说（本刀要验）

生产挂死候选之一：**AIC 已在 WAIT(4) 时空等，AIV 做「真」大体量**（X17）。  
ER01 的 MAC 仅 `64×8`；T04 假循环加压无效（X14）。本刀在 **保持 ER02 EnQue/DeQue + PIPE_V 纪律** 下，把 GATE **真 Vec MAC** 加到近生产量级，看 SIM 是否仍不挂。

## 已锁参数（subagent 不得擅自改）

| 项 | 值 | 说明 |
|----|-----|------|
| 外形 | 与 ER01 相同：Host **2 launch** PREP→COMPUTE；GATE **4/8**；NTT/INTT **1/3** | 禁 5/7 |
| Cube | 仍 `C[16,32]=A[16,32]@B[32,32]` 极轻 | 本刀不加 Cube 体量 |
| GATE MAC | **`kMacElems=256`，`kMacRounds=32`** | 真 Mul/Add/Muls；禁空转 for |
| 同步 | 凡 GM→UB 后 **EnQue/DeQue**；V→S 用 **`PipeBarrier<PIPE_V>`** | 目标 sync_audit **红线 0** |
| 队列 | 输入 **VECIN**、输出 **VECOUT** | 同 ER02 |
| 正确性 | 非门禁 | 不对 liboqs |

若编译/UB 放不下 **256×int32×多缓冲**：先回报主控，**不得**静默改成更小；主控改锁参后再开刀。

## 实现指引

1. **一刀一目录**：从 ER01 目录 **复制脚手架** 到 `ER03-gate-realbrick-volume/`，再改 tiling / MAC / gen_data 尺寸；勿在 ER01 上叠改。  
2. 必读：`STATUS`（ER01/ER02）、`aiv_func.hpp` 中 ER02 同步写法、本任务书。  
3. Host `gen_data` 须按新 `kMacElems` 填满 MAC_A/B/ACC。  
4. TRACE 编号约定不变（见 KB §6 / `trace_map.md`）。

## 必用 cannbot

| 时机 | 动作 |
|------|------|
| 编码后 | `python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py` 扫本刀全部相关源，红线/性能写入 STATUS |
| SIM 挂 | `deadlock-triage.md` + crash-debug；**禁止**用「SIM 绿」否决红线 |

## 永禁

5/7；Wait 中 SyncAll；自造 SoftSync；抄旧 Encrypt；否决 cannbot 红线；假循环当加压；擅自 commit/push/开分支；连 NPU。

## 墙钟

编码 + CPU + `SIM_DIRECT=1` sim ≤ **45min**；超时 **ABORT** 回报（已做/阻塞），不傻等。

## 验收

1. `bash run.sh -r cpu -v Ascend910B4` → SUCCESS / exit 0  
2. `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → SUCCESS；用例根无 stray dump  
3. `sync_audit` **红线 = 0**（性能 SYNC-09/11 可保留并标注）  
4. `STATUS.md` + 反馈块（通/挂/超时 + 学到一条 + tick/wall）

## 回写主控格式

```
ER03: PASS|FAIL|ABORT
dir: graph-tests/enc_related/ER03-gate-realbrick-volume/
cpu: ...
sim: ...
sync_audit: 红线=N ...
blocked: ...
learned: ...
```
