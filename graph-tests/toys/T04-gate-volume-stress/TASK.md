# T04 — GATE 段体量加压（AIC 已 WAIT(4)）

**状态**：PASS（CPU+SIM）  
**图谱**：`D-EXP-T04` → `Q-REPRO-ON-SIM` / `Q-HANG-LOCUS`  
**目录**：`graph-tests/toys/T04-gate-volume-stress/`  
**脚手架**：复制 **T03** 全 FSM 壳；只加大 GATE 段 AIV 体量；禁抄旧 Encrypt

---

## 开刀前遍历（主控）

| 项 | 结论 |
|----|------|
| T01–T03 | 均 PASS；轻量全 FSM（含 INTT 复用 1/3）SIM 不挂 |
| 缺口 | 生产挂死疑与「AIC 已 WAIT(4) 时 AIV 大体量」相关；轻体量尚未加压 |
| 禁令 | 5/7、Wait 中 SyncAll、SoftSync、抄 Encrypt、加 launch 当修复 |
| 非目标 | 数值正确、sampling、多 launch（留给后续） |

---

## 目标

保持 T03 顺序不变：

1. NTT `1/3`  
2. 生产 GATE：AIC **先** `WAIT(4)` → AIV **加重体量** → 双 `SET(4)` → `SET(8)`/`WAIT(8)`  
3. INTT **再** `1/3`（禁 5/7）

**体量要求（本刀核心）**：

- GATE 段 AIV 工作量须明显大于 T03（例如循环轮数/向量长度提升一个数量级，或等价 UB 有界负载）  
- 仍须保证 **单次 SIM kernel 墙钟目标 < 5 min**（预算可写在 run.sh；超时当失败信号记录，勿死磕加到挂死）  
- 不得用 SyncAll / SoftSync「填时间」

## 验收

| # | 标准 |
|---|------|
| A1 | 仅本目录；未改 T01–T03 |
| A2 | CPU `bash run.sh -r cpu -v Ascend910B4` exit 0 |
| A3 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` exit 0，无 Hang |
| A4 | `STATUS.md` 写清相对 T03 的体量参数（轮数/元素数等）与 SIM wall |
| A5 | TRACE 仍能区分 NTT/GATE/INTT；INTT 仍为 1/3 |
| A6 | 墙钟 ≤ **25 min**；SIM ≤ 2；超时/同错 2 → BLOCKED |

若 SIM **Hang/Timeout**：记 FAIL/BLOCKED + 最后 TRACE 号，**禁止**改回 5/7 或对称 GATE 来「修好」。

## 禁止

- flag 5/7；Wait 中 SyncAll；SoftSync；抄 Encrypt；改 yaml；commit/push  
- 把体量加到单次 SIM >5min 还不收敛却继续盲加  
- 回退成对称 GATE 或去掉 INTT 冒充通过

## 参考（只读）

- `graph-tests/toys/T03-full-fsm-ntt-gate-intt/`  
- KB X1–X13；`F-GATE-SEMANTICS-DIFF`

## 反馈

```
T04: PASS|FAIL|BLOCKED
cmd: …
exit: …
wall_min: …
volume: <相对T03的参数>
trace_seen: …
notes: ≤5 行
```

---

## 本刀反馈（回填）

```
T04: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~1.1 (SIM kernel 60.3s；含编译首跑 ~67s)
volume: kRounds=40 (T03=4), kWorkPerAiv=256, touch/AIV=10240 (T03=1024)
trace_seen: 101 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199 (SIM 缺 401)
notes: 生产 GATE 时序不变；AIV 体量×10 后 SIM 仍不挂；INTT 仍复用 1/3；三段 TRACE 可区分；tick 298706 vs T03 49387
```
