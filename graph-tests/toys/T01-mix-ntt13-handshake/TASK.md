# T01 — MIX 最短 NTT 同构握手（flag 1/3）+ TRACE 编号

**状态**：PASS（主控复验；见 STATUS.md）  
**图谱**：`D-EXP-T01` → 服务 `Q-REPRO-ON-SIM` / `D-NEAR-TOYS`（已 inactive，继任 T02）  
**代码目录（本刀唯一）**：`graph-tests/toys/T01-mix-ntt13-handshake/`（实现与本 TASK 同目录或子目录，勿改仓库其它树）

---

## 目标

在 **新实验树** 落地可编译可跑的最短 MIX toy：

1. `KERNEL_TYPE_MIX_AIC_1_2`
2. 仅一段与 Encrypt NTT **同构**的 CrossCore：**AIV SET(1) → AIC WAIT(1) + 极轻 Cube → AIC SET(3) → AIV WAIT(3)**
3. Host 单 launch + SynchronizeStream；打印约定 TRACE 编号
4. **SIM**（及 cpu 若壳需要）能跑完不挂

不对算法正确性；不做 GATE/INTT；不抄旧 Encrypt 核。

## 验收（全部满足才算过）

| # | 标准 |
|---|------|
| A1 | 目录仅本刀新建文件；未改 frozen / stable / 旧 toy |
| A2 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`（或与仓库一致的 sim 入口）**exit 0**，无 Hang/Timeout |
| A3 | 有 `trace_map.md`：编号 ∈ Host100s / AIV0 200s / AIV1 300s / AIC 400s |
| A4 | 运行日志可见至少：Host launch 前编号、AIV SET1、AIC WAIT1后、AIC SET3、Host sync 后编号 |
| A5 | 写 `STATUS.md`：PASS/FAIL、命令、耗时、关键日志路径 |
| A6 | **墙钟 ≤ 30 分钟**；SIM 尝试 ≤ 2；超时或同错 2 次 → **STOP** 并写 BLOCKED 原因（勿死磕） |

## 禁止

- INTT flag 5/7；AIC Wait 中 SyncAll；自造 SoftSync  
- 照抄 `*encrypt*l18*` 生产核；改图谱 yaml / push / commit  
- GATE、多 launch、sampling 大逻辑（留给后续刀）  
- 在旧 `ascendc-tests/pass-toy-*` 上改代码（只可读其 **CMake/run 壳与 CrossCore API 用法**）

## 允许参考（只读）

- `ascendc-tests/fix-toy-encrypt-fsm-ntt1/` 或 `pass-toy-encrypt-fsm-*` 的工程壳与 CrossCore 写法  
- KB：`docs/notes/Encrypt-hang-rewrite-kb.md` §2/§3/§6  
- AscendC 接口查阅索引（写 API 前必查）

## 反馈模板（交回主控）

```
T01: PASS|FAIL|BLOCKED
cmd: …
exit: …
wall_min: …
trace_seen: 101 201 401 …
notes: ≤5 行
```

---

## 本刀反馈（subagent 回填）

```
T01: BLOCKED
cmd: bash run.sh -r cpu -v Ascend910B4  # SIM not reached (wall)
exit: 124
wall_min: 29
trace_seen: 101
notes: Cloud 无 CANN 快照先装 toolkit；CPU TraceMark 与 StubHashSplit TPipe 叠置 abort（已修作用域）；墙钟尽未跑 SIM。详 STATUS.md。
```
